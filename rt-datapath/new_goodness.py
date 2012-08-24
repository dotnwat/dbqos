import itertools
import cPickle as pickle
import numpy as np
import sys
from itertools import combinations_with_replacement as cwr
import copy

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
def ac_goodness(qs, qi, p):
	def sumerr(qw, iops):
		return sum(map(lambda (b,l): l-latency(b, iops), qw))
	seq_iops, idx_iops = p[1], p[4]
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
			compliment = copy.copy(items)
			for val in combo:
				compliment.remove(val)
			union_iter = itertools.chain(combo, compliment)
			assert(sorted(union_iter) == sorted(items))
			yield tuple(combo), tuple(compliment)

#
#
#
def eval_goodness(workload, perf_model):
	partitions = []
	for qs, qi in two_subset_partitions(workload):
		iops_model = perf_model[min(1, len(qs)), len(qi)]
		goodness_val = ac_goodness(qs, qi, iops_model)
		partitions.append((goodness_val, qs, qi))
	return partitions

#
#
#
def run_workloads(output, queries, workload_size, perf_model):
	import time
	x = 0
	start = time.clock()
	for workload in cwr(queries, workload_size):
		partitions = eval_goodness(workload, perf_model)
		results = {'workload': workload, 'partitions': partitions}
		pickle.dump(results, output)
		x += 1
		if x % 100 == 0:
			print x, x / (time.clock() - start) 

#
# Set of queries that will be used to create workloads
#
queries = (
	# 1 GB   time   idx  time
	(262144, 10),  # 0   9.5
	(262144, 60),  # 1   56.9
	(262144, 90),  # 2   87.98
	(262144, 181), # 5   179.8
	(262144, 305), # 9   303
	(262144, 335), # 10  333
	(262144, 5),   # not possible
	(262144, 2),   # not possible
	# 3 blk
	(3,      10),  # could use scan
	(3,      200), # could use scan
	(3,      2),   # needs index
)

best_effort_p = np.load(sys.argv[2])
output = open(sys.argv[1], 'w')
run_workloads(output, queries, 10, best_effort_p)

