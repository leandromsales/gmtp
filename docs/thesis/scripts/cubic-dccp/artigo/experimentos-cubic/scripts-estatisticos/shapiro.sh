#!/bin/sh
    # [environment variables can be set here]

filename=$1

R --slave <<EOF
	arquivo <- read.table("$filename", sep=" ", header=TRUE)
	saida =	shapiro.test(arquivo$"V");
	print(saida)
EOF

