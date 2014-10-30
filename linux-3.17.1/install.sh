#!/bin/bash
cp arch/x86/boot/bzImage /boot/vmlinuz-3.17.1-min
make modules_install
lilo 
#(alterar /etc/lilo.conf)
