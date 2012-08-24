import glob
import numpy as np
import matplotlib.pyplot as plt

BLOCK_SIZE = 4096

#
# data: seq-scan count, idx-scan count,
#       (bw-min, bw-avg, bw-high) for each
#       of the two scan types
#
# 2-dimensions for seq scans (0 and 1)
# 21-dimensions for idx (0-20)
data = np.zeros([2, 21, 6])

#
# Compute IOPS/s (min, mean, max)
#
def make_iops(scans):
	if len(scans) == 0:
		return 0, 0, 0
	scans = scans.flatten('C')
	scan_iops = scans[0::2] / (scans[1::2] / 1000.0)
	return np.min(scan_iops), np.mean(scan_iops), np.max(scan_iops)

#
# Process log into data table
#
def process_log(vals):
	# number of scan types (first two columns)
	seq_scan_cnt, idx_scan_cnt = vals[:,0][0], vals[:,1][0]
	# slice vals into sub-arrays per scan type
	specs, seq_scans, idx_scans = np.hsplit(vals, [2,2+seq_scan_cnt*2])
	# compute bws for each observation (streams * runs)
	seq_scan_iops = make_iops(seq_scans)
	idx_scan_iops = make_iops(idx_scans)
	data[seq_scan_cnt, idx_scan_cnt, :3] = seq_scan_iops
	data[seq_scan_cnt, idx_scan_cnt, 3:] = idx_scan_iops

#
# Injest all of the *.log files
#
input_filenames = glob.glob('*.log')
for filename in input_filenames:
	vals = np.loadtxt(filename, delimiter=' ')
	process_log(vals)

# index-scan iops as a function of 1 seq scans and N idx scans
print data[1,0:,4]

# seq-scan iops as a function idx scan count
print data[1,0:,1]

# index-scan iops as a function of 0 seq scans and N idx scans
print data[0,0:,4]

np.save("bw_0-1seq_0-20rnd_linux", data)
