#!/bin/sh
# [environment variables can be set here]

filename=$1
output=$2
legenda=$3

R --slave <<EOF

file <- read.table("$filename", header=T, sep=" ")
png(filename="$output", height=600, width=600, bg="white")
y <- file$"V"
d <- density(y)
plot(d, main="$legenda")
dev.off()

EOF

