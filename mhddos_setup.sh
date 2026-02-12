#!/bin/sh

if [ `id -u` -ne  0 ]; then
	echo "You must be root to run this script."
	exit 1
fi

# if test -z "$*"; then
# 	# exit 2
# fi

vroot="mhddos"
ULIMITS="--ulimit nofile=10240:10240 --ulimit nproc=65356:65536"

# if test $1 = "-i"; then
# 	vroot=$2
# 	shift 2
# fi

echo "Changing docker image '$vroot'."

# mhinstall='apt -y update && apt -y install curl wget libcurl4 libssl-dev python3 python3-pip make cmake automake autoconf m4 build-essential git && git clone https://github.com/MatrixTM/MHDDoS.git && cd MH* && pip3 install -r requirements.txt'

# apt -y update 
# apt -y install curl wget libcurl4 libssl-dev python3 python3-pip make cmake automake autoconf m4 build-essential git
# git clone https://github.com/MatrixTM/MHDDoS.git
# cd MH*
# pip3 install -r requirements.txt

did=`docker run --detach --tty --net='bridge' $ULIMITS $vroot`
docker exec $did apt update && \
	docker exec -it $did apt -y update &&\
    docker exec -it $did apt -y install curl wget libcurl4 libssl-dev python3 python3-pip python3-venv make cmake automake autoconf m4 build-essential git  &&\
    docker exec -it $did git clone https://github.com/MatrixTM/MHDDoS.git  &&\
    # docker exec -it $did cd MH*  &&\
    docker exec -it $did python3 -m venv xdp-venv  &&\
    docker exec -it $did ./xdp-venv/bin/pip3 install -r ./MHDDoS/requirements.txt  &&\
    # docker exec -it $did $mhinstall && \
	docker exec -it $did apt clean && \
	docker exec -it $did apt autoclean && \
	docker exec -it $did apt autoremove && \
	docker commit $did $vroot

docker kill $did
docker rm -f $did
