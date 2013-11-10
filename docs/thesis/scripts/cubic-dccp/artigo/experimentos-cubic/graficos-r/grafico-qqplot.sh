#!/bin/sh
# [environment variables can be set here]

filename=$1
output=$2

R --slave <<EOF

	file <- read.table("$filename", header=T, sep=" ")
	png(filename="$output", height=600, width=600, bg="white")
	y <- file$"V"
	x <- file$"T"
	qqnorm(y)
	qqline(y, col="red")
	dev.off()
EOF


