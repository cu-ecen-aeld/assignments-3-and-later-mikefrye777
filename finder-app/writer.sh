#!/bin/bash

usage_str="Usage: writer.sh <file path> <string to write>"
# Args Check
if ! [ $# -eq 2 ]
then
    echo "Error: improper number of arguments"
    echo $usage_str
    exit 1
fi

# Behavior
if [ -w $1 ]
then
    echo $2 > $1
    exit 0
elif [ -d $1 ]
then
    echo "Error: cannot write file because directory exists at $1"
    exit 1
else
    mkdir -p $1 # will create the parent directory
    rm -rf $1 # removes the directory with the file's name
    echo $2 > $1
    tmpexit=$?
    if ! [ $tmpexit -eq 0 ]
    then
        echo "Error: could not write to file, $tmpexit"
	exit 1
    else
        exit 0
    fi
fi
