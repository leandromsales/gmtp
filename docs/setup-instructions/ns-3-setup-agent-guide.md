GMTP simulations that execute Linux binaries (`gmtp-server`, `gmtp-client`, `gmtp-inter`) use Direct Code Execution. They do not use a rolling ns-3 checkout.

1. `tools/ns-3-dev-git` is not in this repository. That upstream tree is not the ns-3 that DCE loads.
2. The experiment ns-3 is the revision the DCE guide pins. Official DCE 1.12 uses ns-3.35 with its Linux library. Follow `ns-3-dce-getting-started-agent-guide.md` or, for the 5.10 library, `ns-3-dce-modernization-agent-guide.md`.
3. Scenarios live in `app/ns-3-dce/`. `run_simulation` runs `./waf` in `linux/ns3/linux-net-next-nuse-latest/tools/testing/libos/buildtop/source/ns-3-dce`.
4. The kernel those binaries call is `net/gmtp` inside `linux/ns3/linux-net-next-nuse-latest`, loaded with `--enable-kernel-stack`. An older library copy that also contains GMTP is `linux/ns3/old/linux-net-next-nuse-4.1.0-rc4`.
5. L4S and other native-ns-3 guides keep the checkout they pin. Do not point them at the DCE tree.

## Smoke run after DCE is built

`app/ns-3-dce/README.md` has the one-command path. After Bake has produced
`linux/ns3/linux-net-next-nuse-latest/tools/testing/libos/buildtop/source/ns-3-dce`
and `liblinux.so`:

    cd linux/ns3/linux-net-next-nuse-latest/tools/testing/libos/buildtop/source/ns-3-dce
    ./waf --run dce-gmtp-simple

Or from `app/ns-3-dce/`: `./run_simulation 1 1 2 /tmp/gmtp-dce-smoke`.
A full DCE build and run is heavy; use a Linux VM if the current machine
cannot finish `make library ARCH=lib` or Bake.
