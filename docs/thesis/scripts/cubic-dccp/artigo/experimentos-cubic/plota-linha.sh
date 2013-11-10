#! /bin/sh
file1=$1
leg1=$2
file2=$3
leg2=$4
out=$5

/usr/bin/gnuplot << EOF
	set terminal png;
	set output "$out";

	set title  "Throughput"
	set xlabel "Tempo (segundos)"
	set ylabel "Throughput (Mbps)"
	set key below	


plot "$file1" using 1:2 title "$leg1" with linespoints, "$file2" using 1:2 title "$leg2" with linespoints

EOF


#


