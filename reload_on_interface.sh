#!/bin/bash

echo "requires sudo"

clang -O2 -g -target bpf -c ddos_xdp_kern.bpf.c -o prog.o

ip link set dev eth0 xdpgeneric off

ip link set dev eth0 xdpgeneric obj prog.o sec xdp

# apt-get_imunes -i imunes/ubuntu install libbpf-dev libxdp-dev xdp-tools
# hcp ddos_xdp_kern.bpf.c SERVER:
# mount -t tracefs tracefs /sys/kernel/tracing
# cat /sys/kernel/tracing/trace_pipe

# sudo himage TCP_DDOS
# sudo himage UDP_DDOS
# sudo himage ICMP_DDOS

# source ./xdp-venv/bin/activate

# python ./MHDDoS/start.py udp 10.0.3.10:1200 1 1000
# python ./MHDDoS/start.py tcp 10.0.3.10:1201 1 1000
# python ./MHDDoS/start.py icmp 10.0.3.10 1 1000


# sudo himage CLIENT

# ping 10.0.3.10
