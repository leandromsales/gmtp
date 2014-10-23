#!/bin/sh
    # [environment variables can be set here]

filename1=$1
filename2=$2

R --slave <<EOF

	print("Ordem: TcpCubic CCID-2")
	print("Se o valor de p for MAIOR que 0,05 então o primeiro conjunto é equivalente ao segundo")
	print("")

	file1 <- read.table("$filename1", sep=" ", header=TRUE)
	file2 <- read.table("$filename2", sep=" ", header=TRUE)
	saida =	wilcox.test(file1$"V", file2$"V", paired = F)
	print(saida)
EOF
