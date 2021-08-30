#!/bin/bash

filesdir=$1
#echo ${filesdir}
searchstr=$2


totalfiles=0
linesmatch=0
match=0
##################### ERROR CHECKS ########################
if [ $# -lt 2 ] || [ $# -gt 2 ]
then
   # echo "Argument mismatch. Give the arguments as follows:"
    #echo "arg1 : directory path          arg2:search string parameter"
    exit 1
fi

if [ ! -d $filesdir ]
then
	#echo"directory found"
#else
	#echo "directory NOT found"
	exit 1
fi
#############################################################


########### APPEND /* #############
filesdir="${filesdir}/*"
#echo "${filesdir}"
###################################

function string_search_function() {
	
	grep -c $searchstr $1
}

function loop_and_check_string() {
	if [ -d $1 ]
	then

		for subfile in $1/*
		do

			loop_and_check_string $subfile
		done
	elif [ -f $1 ]
	then
		((totalfiles=totalfiles+1))
		#if grep -c $searchstr $1
		#then
		#	((linesmatch=linesmatch+1))
		#fi
		#match=$(string_search_function $1)
		match= $(grep -R $searchstr $1 | wc -l)
		(( linesmatch=linesmatch+match))
	fi

}

############ LOOP THROUGH DIRECTORY #########################
                echo number of lines  $(grep -R $searchstr $1 | wc -l)
		echo total fiile : $(find $1 -type f | wc -l)
for file in $filesdir
do

	if [ -d $file ]
	then
		file="${file}/*"
		#loop_and_check_string $file
	elif [ -f $file ]
	then
		((totalfiles=totalfiles+1))
		#if [ grep -c $searchstr $file ]
		#then
		#	((linesmatch=linesmatch+1))
		#fi
		# match=$(string_search_function $1)
		match=$(grep -R $searchstr $1 | wc -l)
                (( linesmatch=linesmatch+match))

	fi
done

############################################################

#echo "Total files found : $totalfiles"
#echo "Total lines match : $linesmatch"

printf "The number of files are %d and the number of matching lines are %d" ${totalfiles} ${linesmatch}

