import sys
from random import randint
import numpy as np
import goodness

x="""
Need audit of the goodness fucntion. Need to select latencies
such that they are best in cases other than all going into the sequential scan
bucket.

Use some rounded off bandwidths and do this on paper for a few queries

i think the problem is that the latency calculated for the seq scans is for
some number of index scans concurrently, but when evaluated, finds that the
benefit of having everything executed with zero index scans is greatest. so we
should calculate 

it might be that we have to pre compute the benefit of having everythin in the
seq scan bucket and then adjust accordiningly.

The problem is likely that we get the 5 fold increase in bw from moving things
to the seq bucket with |Q_I|=0. So, we might just need to increase the
magnitude of things.
"""

def rnd_query(maxBlocks, maxDeadline):
	return (randint(1, maxBlocks), randint(1, maxDeadline))

def gen_workload(be_p, wl_size, scan_size):
	qi_size = 1
	qs_size = wl_size - qi_size
	pReal = be_p[min(1, qs_size), qi_size]
	pZeroIdx = be_p[1, 0]
	qi_bw = pReal[4]
	qs_bw = pZeroIdx[1]
	workload = []

	for i in range(1, qi_size+1):
		workload.append((i, i / qi_bw))
	
	for _ in range(1, qs_size+1):
		scan_latency = scan_size / qs_bw / 2
		workload.append((scan_size, scan_latency))

	assert wl_size == len(workload)
	return scan_size, workload

if __name__ == '__main__':
	be_p = np.load(sys.argv[1])
	_, maxrnd, _ = be_p.shape
	rel_size_range = (262144, 524288) # 1-2 GB

	#print be_p[1,::,1]
	#print be_p[0,::,4]
	#print be_p[1,::,4]

	while True:
		scan_size, workload = gen_workload(be_p, 2, 2621440)
		print workload
		partitions = goodness.eval_goodness(workload, be_p, scan_size)
		partitions = sorted(partitions, key=lambda res: res[0], reverse=True)
		for x in partitions:
			print x
		val, qs, qi = partitions[0]
		print len(qs), len(qi)
		if len(qi) > 0:
			break
		break
