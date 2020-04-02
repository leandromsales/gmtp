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

Already in gmtp folder, for example $HOME/gmtp, enter on kernel source directory:

    $ cd linux-5.4.21

Copy config_minimal to .config file:
    
    $ cp config_minimal .config

Run menuconfig to load .config for compile the kernel:

    $ make menuconfig
    
Select save and confirm the .config name and exit.

Now, it's time to compile the kernel itself:

    $ sudo make -j 4

Generate kernel modules:

    $ sudo make modules

## Install Kernel with GMTP code (guest)

The gmtp folder at host must be shared with guest.

In guest, already in shared gmtp folder, for example $HOME/gmtp, enter on kernel source directory:

    $ cd linux-5.4.21

Install modules:

    $ sudo make modules_install
    
Install the new kernel:

    $ sudo make install
    
Shutdown the guest system:

    $ sudo shutdown now

## Building GMTP modules for clients and servers (guest) ##

After enter in guest machines, enter on gmtp project folder, for example:

    $ cd $HOME/gmtp
    
Now navegate to linux-5.4.21/net/gmtp:
    
    $ cd linux-5.4.21/net/gmtp

Compile the code and load gmtp modules:

    $ make
    $ sudo make install

The Makefile is configured to load gmtp and gmtp_ipv4 modules.

## Building GMTP modules for routers/relays (guest) ##

After enter in guest machines, enter on gmtp project folder, for example:

    $ cd $HOME/gmtp

Now navegate to linux-5.4.21/net/gmtp:

    $ cd linux-5.4.21/net/gmtp

Enter on gmtp-inter folder:
    
    $ cd gmtp-inter
    
Compile the code and load the modules:

    $ make
    $ sudo make install

The Makefile is configured to load gmtp and gmtp_inter module.

Note that the gmtp_inter module will only be installed if the gmtp module are previously installed. 


## Running gmtp python examples ##

Navigate to app folder:

    $ cd ~/gmtp/app/python

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
