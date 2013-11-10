#!/bin/sh
# [environment variables can be set here]

filename=$1
output=$2
legenda=$3

R --slave <<EOF

file <- read.table("$filename", header=T, sep=" ")

png(filename="$output", height=600, width=600, bg="white")
boxplot(file$"V", col=c("lightblue"), main="$legenda")
dev.off()

EOF
















