# ddos_xdp
A simple ddos blocker via bpf/xdp written in C.

# INSTALL INSTRUCTIONS 

## IMUNES

### DOWNLOAD
IMUNES-Ubuntu_20250307.ova
from 
https://mrepro.tel.fer.hr/images/IMUNES-Ubuntu_20250307.ova
or 
https://mrepro.tel.fer.hr/images/old/IMUNES-Ubuntu_20250307.ova

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
```
FROM debian:12

COPY debian-12-min/* /build/
```
to
```
FROM ubuntu:24.04
ubuntu-24.04-min/* /build/
```
and

```
FROM debian:12
```
to
```
FROM ubuntu:24.04
```

```
# diff -r ubuntu-24.04-min debian-12-min/
< FROM ubuntu:24.04
> FROM debian:12
< COPY ubuntu-24.04-min/* /build/
> COPY debian-12-min/* /build/

# diff -r ubuntu-24.04 debian-12
< FROM imunes/template:ubuntu-24.04-min
> FROM imunes/template:debian-12-min
< COPY ubuntu-24.04/* /build/
> COPY debian-12/* /build/

Dodati nove direktorij u GNUmakefile:
GNUmakefile
-TAGS = debian-12-min debian-12 arm64 latest-min latest
+TAGS = debian-12-min ubuntu-24.04 ubuntu-24.04-min debian-12 arm64 latest-min latest

U ubuntu-24.04/system_services.sh obrisati ftpd
# make ubuntu-24.04-min
# make ubuntu-24.04
# docker image tag imunes/template:ubuntu-24.04 imunes/ubuntu:latest 
```

```bash
docker images
docker images tag imunes/template:latest imunes/template:latest_backup
```
```bash







