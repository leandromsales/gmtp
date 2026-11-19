# Bake checks for /lib/ld-linux.so.2 (libc6-i386) and the 2.31 debug
# loader (libc6-dbg) before it builds the private glibc.
# Build environment for GMTP under DCE 1.12 (glibc-build), ns-3.35 and the
# Linux 5.10.47 NUSE library. DCE 1.12 was tested on Ubuntu 20.04; this
# image keeps that userland on any Linux host (the AWS VM runs 22.04).
FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential git ca-certificates cmake ninja-build ccache wget curl \
      python3 python3-pip python3-dev python-is-python3 automake autoconf \
      libtool bc bison flex gawk libc6-dbg libdb-dev libssl-dev libpcap-dev \
      libgsl-dev libboost-dev mercurial indent libsysfs-dev rsync gdb \
      pkg-config unzip tar xz-utils patch cvs bzr p7zip-full \
      libxml2-dev libsqlite3-dev libelf-dev texinfo gettext \
      time procps sudo libc6-i386 \
 && python3 -m pip install --no-cache-dir requests distro \
 && rm -rf /var/lib/apt/lists/*
ARG UID=1000
RUN useradd -m -u ${UID} builder && echo 'builder ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/builder
USER builder
WORKDIR /work
