#!/bin/bash


if [[ $# != 1 || ! -d $1 ]]; then
    echo "Usage: ./run_program.sh <directory>"
    exit
fi

./edge_detector $1/*.ppm
