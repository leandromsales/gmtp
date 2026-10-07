# GMTP patches

Read `README.md` in this directory before applying or editing a patch.

These files are an archive of how GMTP was applied to Linux and how the Linux tree was kept in sync with the ns-3 Direct Code Execution kernel in 2015. That library tree is now `linux/ns3/old/linux-net-next-nuse-4.1.0-rc4`. The live protocol code is in `linux/host/linux-net-next-latest` and `linux/ns3/linux-net-next-nuse-latest`. Do not apply these files to update a tree.

Before `git apply` or `patch`, dry-run from the `gmtp/` root and stop on rejects. Do not re-apply a patch whose hunks are already in the tree. New patch text and commit messages are English.
