# GMTP NS-3 / DCE scenarios

These scenarios run under Direct Code Execution 1.12 and ns-3.35, with
`liblinux.so` from `linux/ns3/linux-net-next-nuse-latest`. Do not retarget
them at a newer ns-3, and do not switch the library to LKL.

## Provision (on Linux)

1. Recreate the pinned kernel trees:

       ./linux/checkout-trees.sh

2. Follow `docs/setup-instructions/ns-3-dce-modernization-agent-guide.md`
   (or `ns-3-dce-getting-started-agent-guide.md`) to build Bake, DCE
   tag `dce-1.12` (`18fb3febeb2b9726551cb68346d936fec04e1ff8`), ns-3.35,
   and `liblinux.so` (`make library ARCH=lib` in the NUSE tree).

3. DCE sources end up at:

       linux/ns3/linux-net-next-nuse-latest/tools/testing/libos/buildtop/source/ns-3-dce

   There is no `ns-3-dce` symlink at the repository root. That Bake tree
   stays out of git.

## Smoke run

    ./configure
    ./run_simulation 1 1 2 /tmp/gmtp-dce-smoke

`run_simulation` invokes `./waf --run "dce-gmtp-master ..."` in the DCE
directory. A one-off manual smoke of the smaller example is:

    cd linux/ns3/linux-net-next-nuse-latest/tools/testing/libos/buildtop/source/ns-3-dce
    ./waf --run dce-gmtp-simple

`copy_results` must succeed before traces under the DCE directory are
deleted. Userspace DCE apps use `SOL_GMTP` 300 (`gmtp/gmtp.h` and
`app/include/gmtp.h`).
