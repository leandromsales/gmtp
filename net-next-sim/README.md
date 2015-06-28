A library operating system version of Linux kernel
==================================================

## What is this
This is a library operating system (LibOS) version of Linux kernel,
which will benefit in the couple of situations like:

* operating system personalization
* full featured network stack for kernel-bypass technology (a.k.a. a high-speed packet I/O mechanism) like
Intel DPDK, netmap, etc
* testing network stack in a complex scenario.


## Applications
Right now, we have 2 sub-projects of this LibOS.

- Network Stack in Userspace (NUSE)
_NUSE_ allows us to use Linux network stack as a library which any applications can directory use by linking the library.
Each application has its own network stack so, it provides an instant virtualized environment apart from a host operating system.
- Direct Code Execution (DCE)
_DCE_ provides network simulator integration with Linux kernel so that any Linux implemented network protocols are for a protocol under investigate. Check http://www.nsnam.org/overview/projects/direct-code-execution/ for more detail about DCE.


## Quick start

```x-sh
    cd <gmtp_dir>/net-next-sim
    make defconfig ARCH=lib
    # Optional, to add/remove modules:
    make menuconfig ARCH=lib 
    make library -j8 ARCH=lib
    make testbin -C arch/lib/test
    # Optional, create a link to ns-3-dce source directories:
    ln -s net-next-sim/tools/testing/libos/buildtop/source/ns-3-dce/ ../ 
```

## Aplying patches

```x-sh
    cd <gmtp_dir>
    git checkout <your_branch>
    git diff KernelBase ./linux-4.0.3 >> <path_to_your_patch>.diff
    cd net-next-sim
    make mrproper
    patch -p 2 <  <path_to_your_patch>.diff
    # Repeat steps of quick-start...
```

## Running simulations examples

If you got succeed to build DCE, you can try an example script which is already included in DCE package.

### Example: Simple UDP socket application

```x-sh
    cd gmtp/net-next-sim/tools/testing/libos/buildtop/source/ns-3-dce/
    ./waf --run dce-udp-simple
```
This simulation produces two directories, the content of elf-cache is not important now for us, but files-0 is. files-0 contains first node’s file system, it also contains the output files of the dce applications launched on this node. In the /var/log directory there are some directories named with the virtual pid of corresponding DCE applications. Under these directories there is always 4 files:

1. cmdline: which contains the command line of the corresponding DCE application, in order to help you to retrieve what is it,
2. stdout: contains the stdout produced by the execution of the corresponding application,
3. stderr: contains the stderr produced by the execution of the corresponding application.
4. status: contains a status of the corresponding process with its start time. This file also contains the end time and exit code if applicable.

Before launching a simulation, you may also create files-xx directories and provide files required by the applications to be executed correctly.
Take a look at the output:

```x-sh
    cat files-0/var/log/*/stdout
```

To cleanup output and pcap files:

```x-sh
    rm -rf files-* *.pcap
```

### More details

* for NUSE
https://github.com/libos-nuse/net-next-nuse/wiki/Quick-Start
* for DCE
http://direct-code-execution.github.io/net-next-sim/
