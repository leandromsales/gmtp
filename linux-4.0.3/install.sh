#!/bin/bash
cp arch/x86/boot/bzImage /boot/vmlinuz-4.0.3
make modules_install
lilo 
#(alterar /etc/lilo.conf)
