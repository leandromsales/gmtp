#!/bin/sh
    # [environment variables can be set here]

filename=$1

R --slave <<EOF
	print("=====================Shapiro Test - Sqrt =====================")
	arquivo <- read.table("$filename", sep=" ", header=TRUE)
	saida =	shapiro.test(sqrt(arquivo$"V"));
	print(saida)

	print("")
	print("=====================Shapiro Test - Log =====================")
	saida2 = shapiro.test(log(arquivo$"V"));
	print(saida2)
EOF



