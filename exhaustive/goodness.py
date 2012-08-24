import sys
import itertools
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
#  -  s: size of relation being scanned (blocks)
#
def ac_goodness(qs, qi, p, s):
	seq_iops, idx_iops = p[1], p[4]
	qi_sumerr = sum(map(lambda (b,l): l-latency(b, idx_iops), qi))
	qs_sumerr = sum(map(lambda (b,l): l-latency(s, seq_iops), qs))
	print qi_sumerr, qs_sumerr
	return qs_sumerr + qi_sumerr

def powerset(iterable):
	s = list(iterable)
	return chain.from_iterable(combinations(s, r) for r in range(len(s)+1))

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
# Compute AC goodness for all partitions
#
def eval_goodness(workload, perf_model, scan_size):
	partitions = []
	for qs, qi in two_subset_partitions(workload):
		iops_model = perf_model[min(1, len(qs)), len(qi)]
		goodness_val = ac_goodness(qs, qi, iops_model, scan_size)
		partitions.append((goodness_val, qs, qi))
	return partitions
