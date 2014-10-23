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


	file <- read.table("$filename", sep=" ", header=TRUE)
	print("=====================Z Skewness Test - ====================")
	saida =	skewness(file$"V")/sqrt(6/length(file$"V"))
	print(saida)
	print("")
	print("")
	print("=====================Z Kurtosis Test - =====================")
	saida2 = kurtosis(file$"V")/sqrt(24/length(file$"V"));
	print(saida2)

EOF
