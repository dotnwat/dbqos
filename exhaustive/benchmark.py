import sys
import itertools
import time
import numpy as np

#
# blocks: num 4K blocks
#   iops: 4K blocks/second
#
def latency(blocks, iops):
	return blocks / float(iops)

#
# Average case goodness
#
#  - qs: workload partition using seq-scan
#  - qi: workload partition using idx-scan
#  -  p: feasible operating points (iops)
#
def ac_goodness(qs, qi):
	def sumerr(qw, iops):
		return sum(map(lambda (b,l): l-latency(b, iops), qw))
	seq_iops, idx_iops = 1.0, 1.0
	return sumerr(qs, seq_iops) + sumerr(qi, idx_iops)

#
# Generate 2-subset partitions of the iterable input
#
#  - There is some Knuth-fu that is more efficient!
#
def two_subset_partitions(iterable):
	items = list(iterable)
	for i in range(len(items)+1):
		for combo in itertools.combinations(items, i):
			compliment = items[:]
			for val in combo:
				compliment.remove(val)
			union_iter = itertools.chain(combo, compliment)
			assert(sorted(union_iter) == sorted(items))
			yield tuple(combo), tuple(compliment)

#
# Find optimal goodness value
#
def eval_goodness(workload):
	for qs, qi in two_subset_partitions(workload):
		ac_goodness(qs, qi)

# Time a workload
def runWorkload(workload, repeat):
	start = time.clock()
	for _ in range(repeat):
		eval_goodness(workload)
	return ((time.clock() - start) * 1000.0) / float(repeat)

if __name__ == '__main__':
	for size in range(100):
		workload = [(0, 0)]*size
		elapsed = runWorkload(workload, 3)
		print size, elapsed
