#!/bin/sh

if [ `id -u` -ne  0 ]; then
	echo "You must be root to run this script."
	exit 1
fi

vroot="imunes/ubuntu"
ULIMITS="--ulimit nofile=10240:10240 --ulimit nproc=65356:65536"

echo "Changing docker image '$vroot'."

# clang llvm libelf-dev libpcap-dev build-essential libc6-dev-i386 m4
# linux-tools-$(uname -r)
# linux-headers-$(uname -r)
# linux-tools-common linux-tools-generic
# tcpdump
# libbpf-dev libxdp-dev xdp-tools

did=`docker run --detach --tty --net='bridge' $ULIMITS $vroot`
docker exec $did apt update && \
    docker exec -it $did apt -y install clang llvm libelf-dev libpcap-dev build-essential libc6-dev-i386 m4 &&\
	docker exec -it $did apt -y install linux-tools-$(uname -r) &&\
	docker exec -it $did apt -y install linux-headers-$(uname -r) &&\
	docker exec -it $did apt -y install linux-tools-common linux-tools-generic &&\
	docker exec -it $did apt -y install tcpdump &&\
	docker exec -it $did apt -y install libbpf-dev libxdp-dev xdp-tools &&\
	docker exec -it $did apt clean && \
	docker exec -it $did apt autoclean && \
	docker exec -it $did apt autoremove && \
	docker commit $did $vroot

docker kill $did
docker rm -f $did
