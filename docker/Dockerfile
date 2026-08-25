# syntax=docker/dockerfile:1
# Alpine 3.20 armv7 toolchain — matches README QEMU/chroot flow.
# Produces a musl-linked ARMv7 ELF (websdr.bin). Deploy only if the device
# rootfs uses musl (Alpine-style) or matches this ABI.
FROM --platform=linux/arm/v7 alpine:3.20

RUN apk update && apk add --no-cache \
	openssh-server wpa_supplicant git dhcpcd dnsmasq u-boot-tools hostapd iptables \
	avahi dbus chrony gpsd curl-dev htop frp jq libunwind zlib noip2 noip2-openrc \
	netpbm musl-dev linux-headers g++ gcc cmake make minify fftw-dev fdk-aac-dev \
	pkgconf perl gpsd-dev libunwind-dev zlib-dev sqlite-dev sqlite-static \
	libconfig-static libconfig-dev patch automake autoconf file

COPY docker/web888-build.sh /usr/local/bin/web888-build
RUN chmod +x /usr/local/bin/web888-build

WORKDIR /workspace

ENTRYPOINT ["/usr/local/bin/web888-build"]
CMD ["Release"]
