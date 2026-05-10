# Web-888 Web Server Code

This code is forked from KiwiSDR project.

## The notable changes:

1. Use Linux Kernel Driver to interactive with hardware. No strict timing requirement on code.
1. Use Linux Pthread scheduler to replace userland task scheduler. Add lock protection in many code paths, which was assumed running in single core.
1. Use native thread instead of process for most blocking opertaions.
1. Use hardware GPS instead of Software Defined GPS receiver
1. Use PPS signal to tune ADC clock.
1. Disable ToDA extention for now
1. Use CMake as build system instead of Makefile, gcc as compiler instead of clang.

## Docker build (recommended)

The firmware binary targets **32-bit ARMv7** and links against **musl** (Alpine). On an **x86_64** or **arm64** host, Docker runs the same **Alpine 3.20 armv7** toolchain as the manual QEMU chroot instructions below.

### Prerequisites

- [Docker](https://docs.docker.com/engine/install/) with BuildKit enabled (default on current Docker).
- On **amd64**, register QEMU user-mode handlers **once** so `linux/arm/v7` containers run:

```sh
./docker/install-binfmt.sh
```

(Equivalent: `docker run --rm --privileged tonistiigi/binfmt --install all`.)

### Build

From the repository root (after `git submodule update --init --recursive`):

```sh
./scripts/docker-build.sh
```

This builds **Release** output at **`build-docker/websdr.bin`**.

Options:

- **Debug** (copies web assets without minify; faster iteration):

  ```sh
  ./scripts/docker-build.sh Debug
  ```

- **Clean CMake tree** before configure:

  ```sh
  CLEAN_BUILD=1 ./scripts/docker-build.sh
  ```

- **Different build directory** (default `build-docker`):

  ```sh
  BUILD_DIR=build-my ./scripts/docker-build.sh
  ```

- **Custom image name**:

  ```sh
  WEB888_BUILD_IMAGE=my-web888:dev ./scripts/docker-build.sh
  ```

### Docker Compose

```sh
docker compose build
docker compose run --rm web888-build Release
# or
docker compose run --rm web888-build Debug
```

### Clean up root-owned build trees

The container runs as **root**, so **`build-docker/`** may be owned by root. Remove it with:

```sh
docker run --rm --platform linux/amd64 -v "$(pwd):/w" alpine:3.20 rm -rf /w/build-docker
```

### Deploy

Copy **`websdr.bin`** to the root of the SD card / TF filesystem, replacing the existing binary (see original project notes). If the device uses a **glibc** rootfs, a musl-linked binary from this image may not run; use a matching toolchain or the host’s documented build environment.

---

## Setup a build enviorment by qemu (manual)

The instruction is only tested on Debian. It may work on Ubuntu as well but not testsed.

1. Install necessary packages
```
# sudo apt install qemu-user-static binfmt-support wget
```

2. Create a virtual ARM enviroment (!!not a VM!!)
```
# mkdir ~/alpine
# cd ~/alpine
# wget http://dl-cdn.alpinelinux.org/alpine/v3.20/main/armv7/apk-tools-static-2.14.4-r1.apk
# mkdir alpine-apk
# tar -zxf apk-tools-static-2.14.4-r1.apk --directory=alpine-apk --warning=no-unknown-keyword
# mkdir -p alpine-root/usr/bin
# cp /usr/bin/qemu-arm-static alpine-root/usr/bin/
# mkdir -p alpine-root/etc
# cp /etc/resolv.conf alpine-root/etc/
# cp -r alpine-apk/sbin alpine-root/
# sudo chroot alpine-root /sbin/apk.static --repository http://dl-cdn.alpinelinux.org/alpine/v3.20/main --update-cache --allow-untrusted --initdb add alpine-base
```

Configure apk repo, add main and community channels.
```
# echo http://dl-cdn.alpinelinux.org/alpine/v3.20/main | sudo tee alpine-root/etc/apk/repositories
# echo http://dl-cdn.alpinelinux.org/alpine/v3.20/community | sudo tee -a alpine-root/etc/apk/repositories
```

3. Get into the virtual enviroment, *This command will be used next time after you exist from the virtual enviroment*
```
sudo chroot alpine-root /bin/sh --login
```

run the following commands to install the build tools
```
# apk update
# apk add openssh-server wpa_supplicant git dhcpcd dnsmasq u-boot-tools hostapd iptables avahi dbus chrony gpsd curl-dev htop frp jq libunwind zlib noip2 noip2-openrc netpbm musl-dev linux-headers g++ gcc cmake make minify fftw-dev fdk-aac-dev pkgconf perl gpsd-dev libunwind-dev zlib-dev sqlite-dev sqlite-static libconfig-static libconfig-dev patch automake autoconf
```

4. Inside the virtual enviroment, it is like a normal linux. You can use git to enlist the code, update submodules and use cmake to build the binary.
```
# cd /root
# git clone https://github.com/raspsdr/server
# cd server
# git submodule update --init
# mkdir build
# cd build
# cmake ..
# cmake --build .
```

5. Use the compiled binary websdr.bin to replace the one in the root of your TF card.

6. Happy hack
