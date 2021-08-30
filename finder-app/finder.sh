#!/bin/bash


if [ $# -lt 2 ] || [ $# -gt 2 ]
then
    echo "Argument mismatch. Give the arguments as follows:"
    echo "arg1 : directory path          arg2:search string parameter"
    exit 1

fi

if [ ! -d $filesdir ]
then
    echo "directory NOT found"
    exit 1

fi



printf "The number of files are %d and the number of matching lines are %d \n" $(find $1 -type f | wc -l) $(grep -R $2 $1 | wc -l)
