# ddos_xdp
A simple ddos blocker via bpf/xdp written in C.

# INSTALL INSTRUCTIONS 

## IMUNES

### DOWNLOAD
IMUNES-Ubuntu_20250307.ova
from 
[Curent image](https://mrepro.tel.fer.hr/images/IMUNES-Ubuntu_20250307.ova)
or if not available
[Image in older directory](https://mrepro.tel.fer.hr/images/old/IMUNES-Ubuntu_20250307.ova)

### IMPORT
IMUNES-Ubuntu_20250307.ova as virtual machine

### SETTINGS
4 CPUs

### LOGIN
student
Internet1


## IMAGES

### UPDATES
As sudo
```bash
sudo su
```
Update IMUNES
```bash
cd /root/imunes
git pull
make install
```
Update vroot-linux
```bash
cd /root/vroot-linux
git pull
```
Update entire system and restart
```bash
apt update
apt upgrade
reboot
```
### DOCKER
#### Add Ubuntu images to vroot-linux
(As root)

In /root/vroot-linux copy and rename Debian directories to Ubuntu ones
```bash
cp -r debian-12-min ubuntu-24.04-min
cp -r debian-12 ubuntu-24.04
```

In ubuntu-24.04-min Dockerfile change these lines
```bash
FROM debian:12

COPY debian-12-min/* /build/
```
to
```bash
FROM ubuntu:24.04

ubuntu-24.04-min/* /build/
```

In ubuntu-24.04 Dockerfile change these lines
```bash
FROM imunes/template:debian-12-min

COPY debian-12-min/* /build/
```
to
```bash
FROM imunes/template:ubuntu-24.04-min

COPY ubuntu-24.04/* /build/
```
and remove ftpd from system_services.sh (the last one)

Add ubuntu directories to GNUmakefile
```bash
TAGS = debian-12-min debian-12 arm64 latest-min latest
```
to
```bash
TAGS = debian-12-min ubuntu-24.04 ubuntu-24.04-min debian-12 arm64 latest-min latest
```

### Create new images

~~Backup previous image~~ Not needed
```bash
docker images
docker images tag imunes/template:latest imunes/template:latest_backup
```

Make new ones (min first)
```bash
make ubuntu-24.04-min
make ubuntu-24.04
```

Make new tag
```bash
docker image tag imunes/template:ubuntu-24.04 imunes/ubuntu:latest
```

### Setup DDoS attacker image

Copy image for DDoS attackers
```bash
docker image tag imunes/template:ubuntu-24.04 mhddos:latest
```
Run setup script
```bash
./mhddos_setup.sh
```
or manually install requirements to image
(help on [MHDDoS](https://github.com/MatrixTM/MHDDoS))

### Setup Ubuntu image for XDP







