#!/bin/bash

set -e
set -x

IF=/dev/zero
DD=dd
BS=4K

BASE=$1
NUM_SEQ=$2
NUM_RND=$3

do_gen_zeros() {
    local OF="$BASE.$1.$2.dat"
    $DD if=$IF of=$OF bs=$BS count=$3
}

# generate 8 GB files for plently of
# growing room in doing scans
for (( i = 0; i < $NUM_SEQ; i++)); do
    do_gen_zeros seq $i 2M
done

# generate 1 GB files for random io
for (( i = 0; i < $NUM_RND; i++)); do
    do_gen_zeros rnd $i 256K
done
