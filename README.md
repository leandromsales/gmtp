Global Media Transmission Protocol (GMTP)
=========================================

Repository for the Global Media Transmission Protocol (GMTP).

Contents
========
- Introduction
- Environment
- Install dependencies
- Getting GMTP source 
- Compile Kernel with GMTP code
- Compile and load GMTP modules
- Running example gmtp-apps
- Missing features
- Known Issues


## Introduction ##

This guide intended to help the user to reproduce an environment and test the features of the protocol implemented. Detailing the steps and requirements, how to download from the project repository, compile the kernel, load the required modules and run the sample programs.

## Environment ##

It was used as a base to run a linux environment, using protocol GMTP, a Dell Inspiron 7460 notebook, Intel(R) Core(TM) i7-7500U CPU @ 2.70GHz processor, 16GB of RAM, running Ubuntu 19.10 64bit kernel version 5.3.0-42-generic. The virtual machines were created using Vagrant version 2.2.3, with libvirt provider version 5.4.0 and QEMU API version 5.4.0 (hypersvisor QEMU 4.0.0). Each VM has 512MB of RAM, virtual disk with 30gb and running Ubuntu Server 19.10 64bit, using the "generic/ubuntu1910" vagrant vmbox.

### Install dependencies (Host) ###

For the environment used in the tests, you need to install git to get the source GMTP project, besides the packages for run virtual machines and build the linux kernel.

    $ sudo apt install qemu qemu-kvm libvirt-daemon-system libvirt-clients ebtables dnsmasq-base
    $ sudo apt install libxslt-dev libxml2-dev libvirt-dev zlib1g-dev ruby-dev
    $ sudo apt install git make gcc build-essential flex bison linux-source libtinfo-dev libncurses-dev libelf-dev
    $ sudo apt install kernel-package

ALl dependencies required by guest machines are defined in the Vagrantfile.

## Getting GMTP source ##

Using git to get gmtp source:

    $ git clone https://github.com/compelab/gmtp.git -b linux-5.4.21 --single-branch gmtp

    
## Compile Kernel with GMTP code (host) ##

The host kernel is `linux/host/linux-net-next-latest` (netdev `net-next`, Linux 7.3.0-rc5). GMTP is built into that kernel (`CONFIG_GMTP`, on by default with `INET`). It is not a library kernel.

DCE and NUSE use `liblinux.so` from `linux/ns3/linux-net-next-nuse-latest` (`make library ARCH=lib`). That tree also contains the ported `net/gmtp`. The previous out-of-tree module recipe remains in `linux/host/old/linux-5.4.21`.

From the gmtp/ directory, enter the host kernel:

    $ cd linux/host/linux-net-next-latest

Configure the kernel and leave `CONFIG_GMTP` enabled:

    $ make defconfig
    $ make menuconfig

Compile it:

    $ sudo make -j 4
    $ sudo make modules

## Install Kernel with GMTP code (guest)

The gmtp folder at host must be shared with guest.

In the guest, from the shared gmtp/ directory, enter the kernel source directory:

    $ cd linux/host/linux-net-next-latest

Install modules and the kernel:

    $ sudo make modules_install
    $ sudo make install
    
Shutdown the guest system:

    $ sudo shutdown now

## Building GMTP modules for clients and servers (guest) ##

GMTP for the current host kernel is built with the kernel itself (`CONFIG_GMTP` and `CONFIG_GMTP_INTER` in `linux/host/linux-net-next-latest`). There is no separate `insmod` step for that tree.

The previous loadable-module recipe is `linux/host/old/linux-5.4.21/net/gmtp` (`make` and `sudo make install` there, and `gmtp-inter/` on relays).

## Running gmtp python examples ##

Navigate to app folder:

    $ cd app/python

Run server and client apps:

    $ ./server.py -i eth1 -p 12345 
    
    $ ./client.py -i -a 10.0.1.101 -p 12345
    
Note that you can specify the network interface, ip address and port. Use -h for more info.

Monitoring information through the log file, for example:

    $ /var/log/kern.log


Contributing:
=============

* Implement Linux kernel:
  - http://blog.markloiseau.com/2012/04/hello-world-loadable-kernel-module-tutorial/
* Also in:
  - NS-3: https://www.nsnam.org/
  - OMNet++: http://www.omnetpp.org/
  - OpenWRT: https://openwrt.org/
