#!/bin/bash

writefile=$1
#echo "$writefile"
writestr=$2
#echo "writestr"

##################### ERROR CHECKS ########################
if [ $# -lt 2 ] || [ $# -gt 2 ]
then
    echo "Argument mismatch. Give the arguments as follows:"
    echo "arg1:file with path          arg2:string to be written"
    exit 1
fi

if [ -f $writefile ]
then
	echo "$writestr" > $writefile
else
	touch $writefile
	echo "$writestr" > $writefile
fi
#############################################################


