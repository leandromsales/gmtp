#!/bin/sh
    # [environment variables can be set here]

filename1=$1
filename2=$2

R --slave <<EOF

	print("Ordem: Cubic CCID-2")
	print("Se o valor de p for MENOR que 0,05 então o primeiro conjunto é equivalente ao segundo")
	print("")

	file1 <- read.table("$filename1", sep=" ", header=TRUE)
	file2 <- read.table("$filename2", sep=" ", header=TRUE)
	saida =	t.test(file1$"V", file2$"V", paired = F, alternative='t')
	print(saida)
EOF
