#!/bin/sh
writefile=$1
writestr=$2
if [ $# -ne 2 ]
then
	echo "Parameters were not specified"
	exit 1
fi

if [ ! -e $writefile ] 
then
	dir=$(dirname $writefile)
	if [ ! -d $dir ]
	then
		mkdir $dir 
	fi
	touch $writefile
	if [ ! $? -eq 0 ]
	then
		echo "Failed to create file"
		exit 1
	fi

fi
echo $writestr >> $writefile
