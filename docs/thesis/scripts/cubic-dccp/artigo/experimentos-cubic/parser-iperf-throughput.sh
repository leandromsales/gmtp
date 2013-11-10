#!/bin/bash
file=$1 #Arquivo
header=$2 #tamanho header
segundos=$3 #Numero de segundos pretendidos

num=`wc -l $file | awk '{print $1}'`

#echo $num

value=`expr $num - $header - 1`

tail -n $value $1 | head -n $segundos | awk -F "] " '{print $2}' | awk -F "sec" '{print $1, $2}' | awk -F "-" '{printf "%s %s\n", $1, $2}' | awk '{printf "%s %s\n", $1, $5}'


