#!/bin/bash
if [ $# != 1 ]; then
	echo "Error! usage `basename $0` <logfile>";
	exit 1;
fi

meanFile="${1%\.*}"

echo "load ${meanFile}.log;
stdDev=std(${meanFile});
n=rows(${meanFile});
theMean=mean(${meanFile});
error=(1.96*stdDev)/(theMean*sqrt(n));
delta=error*theMean;
intervaloMin = theMean - delta;
intervaloMax = theMean + delta;
printf(\"Media: %f\n\", theMean);
printf(\"Intervalo: (%f - %f)\n\", intervaloMin, intervaloMax);" > tmp.oct

octave -q tmp.oct;
rm tmp.oct;
