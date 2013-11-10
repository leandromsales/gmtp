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
numberS=((100*1.96*stdDev)/(5*theMean))^2
printf(\"Number of samples: %f\n\", numberS);" > tmp.oct

octave -q tmp.oct;
rm tmp.oct;
