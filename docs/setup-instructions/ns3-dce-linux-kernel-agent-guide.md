The latest Linux kernel officially available for ns-3 via the Direct Code Execution (DCE) framework is Linux Kernel 5.10. [1, 2] 
Historically, DCE was restricted to older kernels like 2.6.36, 3.14, and eventually 4.4. However, the ns-3 consortium officially revitalized the project, upgrading the baseline network stack framework (net-next-nuse) to track Linux 5.10.47 LTS. [2, 3, 4, 5, 6] 
This update is packaged natively with DCE version 1.12 and is officially supported on Ubuntu 20.04 LTS. [1, 7] 
------------------------------
## Core Details of the 5.10 Integration

* 
* The Port Type: It uses the net-next-nuse-5.10 port. This is a partial architecture port rather than a full Linux Kernel Library (LKL) port, allowing it to interface cleanly with ns-3's proprietary discrete fiber-based task scheduler. [2, 3] 
* Feature Support: This version fully unlocks modern protocol mechanics within your simulation—including native IPv4/IPv6, multi-queue alignments, advanced TCP variants (BBR, CUBIC), Multipath TCP (MPTCP), and SCTP—without having to mock them using C++ approximations. [3, 5, 8] 
* Stability Fixes: Because modern glibc builds (version 2.25+) introduced severe security/virtual table checking enhancements that would crash older DCE environments, DCE 1.12 includes a patched version of libc-2.31 designed to bypass these memory hooks within the single-process simulator container. [1, 2, 9] 
* 

## Deploying the 5.10 Kernel Stack
When configuring your layout using the bake build orchestration tool, you can specifically invoke this latest combination using the automated [DCE Modernization track](https://ns-3-dce-linux-upgrade.github.io/) configurations: [2, 10] 

bake.py configure -e dce-linux-1.12
bake.py download
bake.py build

(Note: While some research papers and standalone repositories outline experimental ports of Kernel 6.x architectures for specialized features like TCP Prague, these are custom, third-party forks and are not officially merged or distributed by the upstream nsnam organization).
Are you planning to deploy this natively on an Ubuntu 20.04 host machine, or would you like the configuration commands to spin it up inside an isolated Docker container to avoid host standard library conflicts?

[1] [https://dl.acm.org](https://dl.acm.org/doi/pdf/10.1145/3532577.3532606)
[2] https://ns-3-dce-linux-upgrade.github.io
[3] [https://dl.acm.org](https://dl.acm.org/doi/10.1145/3532577.3532606)
[4] [https://groups.google.com](https://groups.google.com/g/ns-3-users/c/EcH8jBHIHS4)
[5] [https://github.com](https://github.com/direct-code-execution/ns-3-dce/blob/master/doc/source/dce-subprojects.rst)
[6] [https://stackoverflow.com](https://stackoverflow.com/questions/39857295/does-dce-have-its-own-linux-kernel-stack-or-does-it-use-the-host-machines-linux)
[7] [https://ns-3-dce.readthedocs.io](https://ns-3-dce.readthedocs.io/en/latest/getting-started.html)
[8] [https://ns-3-dce.readthedocs.io](https://ns-3-dce.readthedocs.io/en/latest/intro.html)
[9] [https://dl.acm.org](https://dl.acm.org/doi/10.1145/3532577.3532606)
[10] [https://www.nsnam.org](https://www.nsnam.org/workshops/wns3-2022/08-chatterjee-slides.pdf)
