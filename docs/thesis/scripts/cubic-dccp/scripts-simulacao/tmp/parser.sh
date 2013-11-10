#!/bin/bash

fileName=$1
output=$2

cat $fileName | grep dccp_probe | grep -v "0 0 0 0 0 0 0" | grep -v "printk[0]"| awk -F ") " '{print $1 $2}' | awk -F "]: " '{print $2}' | awk -F "<6>dccp_probe<=>" '{printf "%s %s\n", $1, $2}' > $output
