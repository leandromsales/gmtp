#!/bin/sh
# [environment variables can be set here]

filename1=$1
leg1=$2
filename2=$3
leg2=$4
output=$5


R --slave <<EOF

	png(filename="$output", height=600, width=600, bg="white")

	x <- read.table("$filename1", header=T, sep=" ")$"V"
	y <- read.table("$filename2", header=T, sep=" ")$"V"
	g_range <- range(0, x, y)
	boxplot(x, y, col=c("lightblue", "red"), main="Boxplot Comparativo")
	legend(1.3, g_range[2], c("$leg1", "$leg2"), cex=0.8, col=c("lightblue","red"), pch=21:22);
	dev.off()

EOF
