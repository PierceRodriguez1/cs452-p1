#!/usr/bin/env bash

# Display usage information
function usage() {
    echo "$0 usage:" && grep " .)\ #" $0
    exit 0
}

# Check if arguments are provided
[ $# -eq 0 ] && usage

# Parse command-line options
while getopts "hs:f:" arg; do
    case $arg in
    s) # The size of the array to sort
        size=${OPTARG}
        ;;
    f) # The plot file name
        name=${OPTARG}
        ;;
    h | *) # Display help
        usage
        exit 0
        ;;
    esac
done

# Check if required arguments are provided
if [ -z "$name" ] || [ -z "$size" ]; then
    usage
    exit 0
fi

# Ensure myprogram exists and has been compiled
if [ -e ./myprogram ]; then
    # Remove existing data file if it exists
    if [ -e "data.dat" ]; then
        rm -f data.dat
    fi

    # Generate data by running myprogram with different thread counts
    echo "Running myprogram to generate data"
    echo "#Time Threads" >> data.dat
    for n in {1..32}; do
        echo -ne "running with $n thread(s)\r"
        ./myprogram "$size" "$n" >> data.dat
    done

    # Use gnuplot to create a graph
    gnuplot -e "filename='$name.png'" graph.plt
    echo "Created plot $name.png from data.dat file"
else
    echo "myprogram is not present. Did you compile your code?"
fi
