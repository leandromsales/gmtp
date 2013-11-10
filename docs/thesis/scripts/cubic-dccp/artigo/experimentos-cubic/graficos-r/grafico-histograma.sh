#!/bin/sh
# [environment variables can be set here]

filename=$1
output=$2
legenda=$3
xlegenda=$4

R --slave <<EOF

file <- read.table("$filename", header=T, sep=" ")
png(filename="$output", height=600, width=600, bg="white")
x <- file$"V"
h <- hist(x, col="lightblue", ann=FALSE)
title(main="$legenda", col.main="blue", font.main=4)
title(ylab="Frequência", xlab="$xlegenda")
dev.off()

EOF
