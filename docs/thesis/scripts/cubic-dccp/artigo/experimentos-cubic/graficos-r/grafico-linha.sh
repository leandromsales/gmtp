#!/bin/sh
# [environment variables can be set here]

file1=$1
leg1=$2
file2=$3
leg2=$4
output=$5

R --slave <<EOF

	png(filename="$output", height=600, width=600, bg="white")

	x <- read.table("$file1", header=T, sep=" ")$"V"
	y <- read.table("$file2", header=T, sep=" ")$"V"
	g_range <- range(0, 0, 1)

	plot(y, type="l", col="blue", ann=FALSE, pch=21, ylim=g_range)
	lines(x, type="l", col="red", ann=FALSE, pch=22)
	title(main="Vazão", col.main="blue", font.main=4)
	title(ylab="Vazão (Mbps)", xlab="Tempo(s)")
	legend(150, g_range[2], c("$leg1","$leg2"), cex=0.8, col=c("red", "blue"), pch=21:22, lty=1:4);
	dev.off()

EOF

