#!/bin/sh
filesdir=$1
searchstr=$2
if [ $# -ne 2 ]
then
	echo "Parameters were not specified"
	exit 1
fi

if [ -d $filesdir ] 
then
	list=`grep -irc $searchstr $filesdir | sed '/:0/d'` 
	count=0
	numFiles=0
	for line in $list
	do
		num=`cut -d ':' -f2 <<< $line`
		count=$((count+num))
	done
	if [ $count -gt 0 ]
	then
		numFiles=`wc -l <<< $list`
	fi
	echo "The number of files are $numFiles and the number of matching lines are $count"
else
	echo "File not found"
	exit 1
fi
