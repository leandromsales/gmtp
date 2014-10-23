#!/bin/sh
    # [environment variables can be set here]

filename=$1


R --slave <<EOF
	"skewness" <-
	function (x, na.rm = FALSE) 
	{
	    if (is.matrix(x)) 
		apply(x, 2, skewness, na.rm = na.rm)
	    else if (is.vector(x)) {
		if (na.rm) x <- x[!is.na(x)] 
		n <- length(x)
	     (sum((x-mean(x))^3)/n)/(sum((x-mean(x))^2)/n)^(3/2)
		}
	    else if (is.data.frame(x)) 
		sapply(x, skewness, na.rm = na.rm)
	    else skewness(as.vector(x), na.rm = na.rm)
	}

	"kurtosis" <-
	function (x, na.rm = FALSE) 
	{
	    if (is.matrix(x)) 
		apply(x, 2, kurtosis, na.rm = na.rm)
	    else if (is.vector(x)) {
		if (na.rm) x <- x[!is.na(x)] 
		n <- length(x)
		n*sum( (x-mean(x))^4 )/(sum( (x-mean(x))^2 )^2)
		}
	    else if (is.data.frame(x)) 
		sapply(x, kurtosis, na.rm = na.rm)
	    else kurtosis(as.vector(x), na.rm = na.rm)
	}


	arquivo <- read.table("$filename", sep=" ", header=TRUE)

	print("=====================Skewness Test - =====================")
	saida =	skewness(arquivo$"V");
	print(saida)
	print("")
		
	print("=====================Kurtosis Test - =====================")
	saida2 = kurtosis(arquivo$"V");
	print(saida2)

EOF

