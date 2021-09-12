#!/bin/sh

#DIRECTORY - ARGUMENT 1
filesdir=$1

#STRING TO MATCH - ARGUMENT 2
searchstr=$2

# VARIABLE THAT INCREMENTS WHEN A MATCH IS FOUND IN A PARTICULAR FILE
totalfiles=0

#VARIABLE TO CAPTURE TOTAL LINES CONTAINING SEARCHSTRING
linesmatch=0

#TEMPORARY VARIABLE THAT HOLDS MATCHING LINES COUNT FOR A SINGLE FILE
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



############## FUNCTION THAT IS CALLED RECURSIVELY TO TRAVERSE THROUGH SUBFOLDERS AND SEARCH STRING ##############
loop_and_check_string() {
        if [ -d $1 ]
        then

                for subfile in $1/*
                do

                        loop_and_check_string $subfile
                done
        elif [ -f $1 ]
        then
                match=$(grep -c $searchstr $1)
		if [ $match -gt 0 ]
		then
			((totalfiles=totalfiles+1))
		fi
                (( linesmatch=linesmatch+match))
        fi

}


############ CALL FUNCTION TO START SEARC #########################
#loop_and_check_string $filesdir
#totalfiles=find $filesdir -type f | wc -l
#linesmatch=grep -R $searchstr $filesdir | wc -l


################ PRINT TOTALFILES AND LINESMATCH #####################
#printf "The number of files are %d and the number of matching lines are %d" ${totalfiles} ${linesmatch}
#printf "The number of files are %d and the number of matching lines are %d" find $filesdir -type f | wc -l grep -R $searchstr $filesdir | wc -l
echo "The number of files are $(find $1 -type f | wc -l) and the number of matching lines are $(grep -R $2 $1 | wc -l)" #instead of functions we can directly use this line to get the results

