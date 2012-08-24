#!/bin/bash

set -e
set -x

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PATH=$PATH:$DIR

run_combos() {
	for i in {0..20}
	do
		OUTFILE="$1-$i.log"
		echo 1 > /proc/sys/vm/drop_caches
		workload -s $1 -x $i -b input >> $OUTFILE
	done
}

run_combos 0
run_combos 0
run_combos 0
run_combos 1
run_combos 1
run_combos 1
