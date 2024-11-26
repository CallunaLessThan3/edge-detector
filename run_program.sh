#!/bin/bash

USAGE="Usage: ./run_program.sh <directory>"

if [[ $# != 1 || ! -d $1 ]]; then
    echo $USAGE
    exit
fi

ARGS=$1/*.ppm
./edge_detector $ARGS
