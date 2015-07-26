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

It was used as a base to run a linux environment, using protocol GMTP, a Dell Vostro 5470 notebook, core i5 4200u processor, 4GB of RAM, running Ubuntu 15.04 64bit kernel version 3.19.0-22-generic. A virtual machine was created using VirtualBox version 4.3.28 r100309 with extension pack installed, 512MB of RAM, virtual disk with 30gb and running Ubuntu Server 14.04.2LTS 64bit only with the base system installed.

### Install dependencies ###

For the environment used in the tests, you need to install git to get the source GMTP project, besides the packages make, gcc, libncurses5-dev and lilo(if you prefer instead GRUBs).

    # apt-get install git make gcc libncurses5-dev lilo


## Getting GMTP source ##

Using git to get gmtp source:

    $ git clone https://github.com/compelab/gmtp.git -b master

    
## Compile Kernel with GMTP code ##

Already in gmtp folder, for example ~/gmtp, enter on kernel source directory:

    $ cd linux-4.0.3

Copy config_minimal to .config file:
    
    $ cp config_minimal .config

Run menuconfig to load .config for compile the kernel:

    $ make menuconfig
    
Select save and confirm the .config name and exit.

Note: In some cases you will need to disable the initial option "Initial RAM filesystem and RAM disk", because it can cause kernel panic. To make this change do it:

Select General setup option and press return, now make sure that the option "Initial RAM filesystem and RAM disk" is deselected(use space bar to slect or not).

Select save and confirm the .config name, exit and exit again.


Now, it's time to compile the kernel itself:

    $ make -j 4

Generate kernel modules:

    $ make modules

Install modules:

    # make modules_install
    
Install the new kernel:

    # make install
    
If you prefer to use Lilo,follow these steps:

Generate lilo config file:

    # liloconfig
    
Edit lilo.conf:

    # vim /etc/lilo.conf
    
and if there is no entry for the new kernel, please add:

image = /boot/vmlinuz-4.0.3
    label = "linux-4.0.3"
    root = /dev/sda1

Run lilo:
    
    # lilo
    

- Compile and load GMTP modules

Enter on gmtp project folder, for example:

    $ cd ~/gmtp
    
Now navegate to linux-4.0.3/net/gmtp:
    
    $ cd linux-4.0.3/net/gmtp

Compile the code and load gmtp modules:

    $ make && sudo make install

The Makefile is configured to load gmtp and gmtp_ipv4 modules.

Enter on gmtp-inter folder:
    
    $ cd gmtp-inter
    
Compile the code and load the modules:

    $ make && sudo make install

The Makefile is configured to load gmtp and gmtp_inter module.

Note that the gmtp_inter module will only be installed if the gmtp and gmtp_ipv4 modules are previously installed. 


## Running gmtp python examples ##

Navigate to app folder:

    $ cd ~/gmtp/app/python

Run server and client apps:

    $ ./server.py -i eth0 -a 10.0.2.15 -p 12345
    
    $ ./client.py -i eth0 -a 10.0.2.15 -p 12345
    
Note that you can specify the network interface, ip address and port. Use -h for more info.

Monitoring information through the log file:

    $ tail -f /var/log/syslog


Contributing:
=============

* Implement Linux kernel:
  - http://blog.markloiseau.com/2012/04/hello-world-loadable-kernel-module-tutorial/
* Also in:
  - NS-3: https://www.nsnam.org/
  - OMNet++: http://www.omnetpp.org/
  - OpenWRT: https://openwrt.org/
