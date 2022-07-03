#!/bin/bash

error_str="Usage: finder.sh <directory path> <string to search>"

# Arguments check
if ! [ $# -eq 2 ] 
then
    echo "Error: improper number of arguments"
    echo $error_str
    exit 1
fi

if ! [ -d $1 ]
then
    echo "Error: specified path is not a directory"
    echo $error_str
    exit 1
fi


# Behavior
num_files=$(find $1 -type f | wc -l)
num_matches=$(find $1 -type f -exec grep $2 {} \; | wc -l)
echo "The number of files are $num_files and the number of matching lines are $num_matches"
