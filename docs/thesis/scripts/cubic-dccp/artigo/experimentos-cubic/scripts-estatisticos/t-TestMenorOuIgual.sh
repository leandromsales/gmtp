#!/bin/sh
    # [environment variables can be set here]

filename1=$1
filename2=$2

R --slave <<EOF

	print("Ordem: Cubic, CCID-2")
	print("Se o valor de p for menor que 0,05 então o primeiro conjunto é maior que o segundo")
	print("TESTAR ESSE VALOR PRIMEIRO")
	print("")

	file1 <- read.table("$filename1", sep=" ", header=TRUE)
	file2 <- read.table("$filename2", sep=" ", header=TRUE)
	saida =	t.test(file1$"V", file2$"V", paired = F, alternative='g')
	print(saida)
EOF

