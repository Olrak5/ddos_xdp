#include <linux/bpf.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#include <stdbool.h>
#include <string.h>
#include <xdp/parsing_helpers.h>


#define XDP_ACTION_MAX (XDP_REDIRECT + 1)

#define NANOSEC_PER_SEC 1000000000ULL /* 10^9 */

#define UPDATE_PERIOD_SEC 2ULL // in seconds

#define UPDATE_PERIOD UPDATE_PERIOD_SEC * NANOSEC_PER_SEC

#define DISBLE_TIME_N  3ULL

#define DISBLE_TIME DISBLE_TIME_N * UPDATE_PERIOD

#define DEBUG_LEVEL 1 // 0 - 3

#define MAX_POSSIBLE_CPUS 4

#define MAX_ALLOWED_PACKETS_PER_SECOND 400ULL

#define MAX_ALLOWED_BYTES_PER_SECOND MAX_ALLOWED_PACKETS_PER_SECOND * 500ULL

enum proto_type {
    ETH = 0,
    IPV4,
    IPV6,
    ICMP,
    TCP,
    UDP,
    OTHER_ETH,
    OTHER_IP,
    OTHER,
};

#define PROTO_MAX (OTHER + 1)

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, __u32);
	__type(value, __u64);
	__uint(max_entries, 1);
} chosen_cpu SEC(".maps");

struct proto_traffic_info {
    __u64 packet_count;
    __u64 packet_size_sum;
};

struct net_traffic_info {
    struct proto_traffic_info info_per_proto[PROTO_MAX];
};

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__type(key, __u32);
	__type(value, struct net_traffic_info);
	__uint(max_entries, 1);
} recent_packet_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} timer_start_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, PROTO_MAX);
    __type(key, __u32);
    __type(value, __u32);
} proto_action_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, PROTO_MAX);
    __type(key, __u32);
    __type(value, __u64);
} proto_last_attack_time_map SEC(".maps");


static __always_inline
void init_chosen_cpu()
{
    __u32 key = 0;
    __u64 value = 0;
    __u64 *cpu_is_chosen = bpf_map_lookup_elem(&chosen_cpu, &key);

    if (cpu_is_chosen == NULL)
        bpf_map_update_elem(&chosen_cpu, &key, &value, 0);
}

static __always_inline
__u32 make_chosen_cpu()
{
    __u32 key = 0;
    __u64 *cpu_is_chosen = bpf_map_lookup_elem(&chosen_cpu, &key);

    if (cpu_is_chosen == NULL)
        return -1;

    return __sync_val_compare_and_swap(cpu_is_chosen, (__u64)0, (__u64)1);
}

static __always_inline
void reset_chosen_cpu()
{
    __u32 key = 0;
    __u64 value = 0;
    __u64 *cpu_is_chosen = bpf_map_lookup_elem(&chosen_cpu, &key);

    if (cpu_is_chosen == NULL)
    {
        bpf_map_update_elem(&chosen_cpu, &key, &value, 0);
        return;
    }
    else
    {
        __sync_fetch_and_and(cpu_is_chosen, (__u64)0);
    }
}

static __always_inline
__u32 timer_reset(__u64 start_time)
{
    __u32 key = 0;
    __u64 *timer_start = bpf_map_lookup_elem(&timer_start_map, &key);

    if (!timer_start)
        bpf_map_update_elem(&timer_start_map, 
                            &key, 
                            &start_time, 
                            0);
    else
        *timer_start = start_time;

    return 0;

}

static __always_inline
__u64 get_start_time()
{
    __u32 key = 0;
    __u64 *timer_start = bpf_map_lookup_elem(&timer_start_map, &key);

    if (!timer_start)
        return -1;

    return *timer_start;
}

static __always_inline
__u32 update_packet_map(__u64 packet_size, __u32 nh_proto_enum)
{
    __u32 key = 0;
	struct net_traffic_info *net_info = 
        bpf_map_lookup_elem(&recent_packet_map, &key);

    if (!net_info)
    {
        struct net_traffic_info new_net_info;
        __builtin_memset(&new_net_info, 0, sizeof(new_net_info));
        bpf_map_update_elem(&recent_packet_map, 
                            &key, 
                            &new_net_info, 
                            0);

        net_info = bpf_map_lookup_elem(&recent_packet_map, &key);
        if (!net_info)
            return -1;
    }

    net_info->info_per_proto[nh_proto_enum].packet_count++;
    net_info->info_per_proto[nh_proto_enum].packet_size_sum += packet_size;

    return 0;
}

static __always_inline
__u32 set_attack(__u32 proto, __u64 current_time)
{
    __u64 *last_attack_time = 
        bpf_map_lookup_elem(&proto_last_attack_time_map, &proto);

    bpf_map_update_elem(&proto_last_attack_time_map, 
                        &proto, 
                        &current_time, 
                        0);

    return 0;
}

static __always_inline
__u64 get_last_attack_time(__u32 proto)
{
    __u64 *last_attack_time = 
        bpf_map_lookup_elem(&proto_last_attack_time_map, &proto);

    if (!last_attack_time)
        return 0;
    else
        return *last_attack_time;
}

static __always_inline
__u32 get_action_from_map(enum proto_type last_proto , __u32 *action_in)
{
    __u32 *action = bpf_map_lookup_elem(&proto_action_map, &last_proto);

    if (action == NULL)
    {
        *action_in = XDP_PASS;
        return 1;
    }

    *action_in = *action;

    return 0;
}

static __always_inline
__u32 disable_proto(__u32 proto)
{
    __u32 *action = bpf_map_lookup_elem(&proto_action_map, &proto);

    if (action == NULL)
    {
        __u32 new_action = XDP_DROP;

        bpf_map_update_elem(&proto_action_map, 
                            &proto, 
                            &new_action, 
                            0);
        
        return 1;
    }
    else 
    {
        *action = XDP_DROP;

        return 0;
    }
}

static __always_inline
__u32 enable_proto(__u32 proto)
{
    __u32 *action = bpf_map_lookup_elem(&proto_action_map, &proto);

    if (action == NULL)
    {
        __u32 new_action = XDP_PASS;

        bpf_map_update_elem(&proto_action_map, 
                            &proto, 
                            &new_action, 
                            0);
        
        return 0;
    }
    
    if (*action == XDP_DROP)
    {
        *action = XDP_PASS;
        return 1;
    }
    else
    {
        *action = XDP_PASS;
        return 0;
    }
}

static __always_inline
bool check_for_attack(struct net_traffic_info *all_cpu_traffic_info, __u32 proto)
{
    __u64 up = (__u64)UPDATE_PERIOD_SEC;
    __u64 pps = all_cpu_traffic_info->info_per_proto[proto].packet_count 
                    / up;
    __u64 bps = all_cpu_traffic_info->info_per_proto[proto].packet_size_sum
                    / up;

    if (DEBUG_LEVEL > 1)
        bpf_printk("proto %u pps %u > max %u bps %u > max %u", 
            proto, pps, MAX_ALLOWED_PACKETS_PER_SECOND, bps, MAX_ALLOWED_BYTES_PER_SECOND);
    
    if (pps > MAX_ALLOWED_PACKETS_PER_SECOND 
            || bps > MAX_ALLOWED_BYTES_PER_SECOND)
    {
        return true;
    }

    return false;
}

static __always_inline
__u32 reset_packet_map()
{
    __u32 key = 0;

    struct net_traffic_info new_net_info;
    __builtin_memset(&new_net_info, 0, sizeof(new_net_info));
    bpf_map_update_elem(&recent_packet_map, 
                        &key, 
                        &new_net_info, 
                        0);

    return 0;
}

static __always_inline
void collect_percpu_traffic_info(struct net_traffic_info *all_cpu_traffic_info)
{
    __u32 cpu_i;
    __u32 proto_i;
    __u32 key = 0;

    for (cpu_i = 0; cpu_i < MAX_POSSIBLE_CPUS; cpu_i++)
    {
        struct net_traffic_info *single_cpu_traffic_info = 
            bpf_map_lookup_percpu_elem(&recent_packet_map, &key, cpu_i);

        if (!single_cpu_traffic_info)
            continue;

        for (proto_i = 0; proto_i < PROTO_MAX; proto_i++)
        {
            all_cpu_traffic_info->info_per_proto[proto_i].packet_count 
                += single_cpu_traffic_info->info_per_proto[proto_i].packet_count;
            all_cpu_traffic_info->info_per_proto[proto_i].packet_size_sum 
                += single_cpu_traffic_info->info_per_proto[proto_i].packet_size_sum;

            
            __sync_fetch_and_and(&single_cpu_traffic_info->info_per_proto[proto_i].packet_count, 0ULL);
            __sync_fetch_and_and(&single_cpu_traffic_info->info_per_proto[proto_i].packet_size_sum, 0ULL);


            if (DEBUG_LEVEL > 2)
            {
                bpf_printk("sum from cpu %u, proto %u", cpu_i, proto_i);
                bpf_printk("pc %u, ps %u", 
                    single_cpu_traffic_info->info_per_proto[proto_i].packet_count, 
                    single_cpu_traffic_info->info_per_proto[proto_i].packet_size_sum
                    );
            }

            
        }
    }
}

static __always_inline
void parse_packet(
                    void *data_end, 
                    void *data, 
                    struct hdr_cursor *nh_in,
                    __u32 *action_in,
                    enum proto_type *last_proto_in
                )
{
    struct hdr_cursor nh = *nh_in;

    struct ethhdr *ethhdr;
    struct iphdr *iphdr;
    struct ipv6hdr *ipv6hdr;
    struct icmphdr_common *icmphdr_c;
    struct tcphdr *tcphdr;
    struct udphdr *udphdr;

    __u32 eth_proto;
    __u32 ip_proto;
    __u32 icmp_type = -1;
    __u32 tcp_len;
    __u32 udp_len;

    enum proto_type last_proto;

    __u64 packet_size = data_end - data;

    __u32 action = XDP_PASS;

    eth_proto = parse_ethhdr(&nh, data_end, &ethhdr);
    last_proto = ETH;

    if (eth_proto < 0) 
    {
        action = XDP_DROP;
        goto out;
    }

    if (eth_proto == bpf_htons(ETH_P_IP))
    {
        ip_proto = parse_iphdr(&nh, data_end, &iphdr);
        last_proto = IPV4;

        if (ip_proto < 0) 
        {
            action = XDP_DROP;
            goto out;
        }
    } else if (eth_proto == bpf_htons(ETH_P_IPV6))
    {
        ip_proto = parse_ip6hdr(&nh, data_end, &ipv6hdr);
        last_proto = IPV6;

        if (ip_proto < 0) 
        {
            action = XDP_DROP;
            goto out;
        }
    } else 
    {
        last_proto = OTHER_ETH;
        goto out;
    }


    if (eth_proto == bpf_htons(ETH_P_IP) && ip_proto == IPPROTO_ICMP) 
    {
        icmp_type = parse_icmphdr_common(&nh, data_end, &icmphdr_c);
        last_proto = ICMP;

        if (icmp_type < 0) 
        {
            action = XDP_DROP;
            goto out;
        }
    } else if (eth_proto == bpf_htons(ETH_P_IPV6) && ip_proto == IPPROTO_ICMPV6) 
    {
        icmp_type = parse_icmphdr_common(&nh, data_end, &icmphdr_c);
        last_proto = ICMP;

        if (icmp_type < 0) 
        {
            action = XDP_DROP;
            goto out;
        }
    } else if (ip_proto == IPPROTO_TCP)
    {
        tcp_len = parse_tcphdr(&nh, data_end, &tcphdr);
        last_proto = TCP;

        if (tcp_len < 0) 
        {
            action = XDP_DROP;
            goto out;
        }
    } else if (ip_proto == IPPROTO_UDP)
    {
        udp_len = parse_udphdr(&nh, data_end, &udphdr);
        last_proto = UDP;

        if (udp_len < 0) 
        {
            action = XDP_DROP;
            goto out;
        }
    } else 
    {
        last_proto = OTHER_IP;
        goto out;
    }

out:

    *nh_in = nh;
    *action_in = action;
    *last_proto_in = last_proto;
}

SEC("xdp")
__u32 xdp_ddos_detect_and_drop(struct xdp_md *ctx)
{
    __u64 current_time = bpf_ktime_get_ns();

    __u32 cpu_id = bpf_get_smp_processor_id();

    __u64 start_time = get_start_time();

    if (start_time <= 0)
    {
        timer_reset(current_time);
        return XDP_PASS;
    }        

    init_chosen_cpu();

    __u64 time_d = current_time - start_time;

	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;

    struct hdr_cursor nh;
    nh.pos = data;

    struct ethhdr *ethhdr;
    struct iphdr *iphdr;
    struct ipv6hdr *ipv6hdr;
    struct icmphdr_common *icmphdr_c;
    struct tcphdr *tcphdr;
    struct udphdr *udphdr;

    __u32 nh_proto;
    __u32 eth_proto;
    __u32 ip_proto;
    __u32 icmp_type = -1;
    __u32 tcp_len;
    __u32 udp_len;

    enum proto_type last_proto;

    __u64 packet_size = data_end - data;

    __u32 action;

    parse_packet(
        data_end, 
        data,
        &nh,
        &action,
        &last_proto
    );

    update_packet_map(packet_size, last_proto);

    if(time_d > UPDATE_PERIOD && make_chosen_cpu() == 0)
    {
        if (DEBUG_LEVEL > 2)
            bpf_printk("time passed on %u ", cpu_id);

        timer_reset(current_time);

        struct net_traffic_info all_cpu_traffic_info;
        __builtin_memset(&all_cpu_traffic_info, 0, sizeof(all_cpu_traffic_info));

        collect_percpu_traffic_info(&all_cpu_traffic_info);

        // CALCULATE AND SET ACTIONS PER PROTO

        for (__u32 proto_i = 0; proto_i < PROTO_MAX; proto_i++)
        {
            if (check_for_attack(&all_cpu_traffic_info, proto_i))
            {
                if (DEBUG_LEVEL > 0)
                    bpf_printk("attack on %u ", proto_i);
                set_attack(proto_i, current_time);
                disable_proto(proto_i);

            }
            else {                
                __u64 last_attack_time = get_last_attack_time(proto_i);

                if (last_attack_time == 0 ||
                    current_time - last_attack_time > DISBLE_TIME)
                {
                    __u32 was_disabled = enable_proto(proto_i);
                    
                    if (was_disabled == 1 && last_attack_time > 0 && DEBUG_LEVEL > 0)
                    {
                        bpf_printk("attack on %u stopped", proto_i);
                    }
                        
                }
                else if (last_attack_time > 0 && 
                        current_time - last_attack_time <= DISBLE_TIME && 
                        DEBUG_LEVEL > 0)
                {
                    bpf_printk("waiting for attack timeout for %d s", (DISBLE_TIME - (current_time - last_attack_time))/NANOSEC_PER_SEC );
                }
            }
        }

        if (DEBUG_LEVEL > 1) bpf_printk("");

        reset_chosen_cpu();
    }

    if (action == XDP_DROP)
    {
        if (DEBUG_LEVEL > 2)
            bpf_printk("bad packet drop on proto %u", last_proto);
        goto out;
    }

    get_action_from_map(last_proto ,&action);

    if (DEBUG_LEVEL > 2)
        bpf_printk("action %u from map on proto %u", action, last_proto);
    
out:

    if (DEBUG_LEVEL > 2)
    {
        if (action == XDP_DROP)
        {
            bpf_printk("dropped %u packet", last_proto);
        }
        else 
        {
            bpf_printk("passed %u packet", last_proto);
        }
    }

    return action;
}

char _license[] SEC("license") = "GPL";