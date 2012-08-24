import glob
import numpy as np
import matplotlib.pyplot as plt

BLOCK_SIZE = 4096

#
# data: seq-scan count, idx-scan count,
#       (bw-min, bw-avg, bw-high) for each
#       of the two scan types
#
# seq-scan is always 1 for this experiment
# and we don't put anything in the 'zero'
# dimension.
#
data = np.zeros([2, 11, 6])

#
# Return MB/s min, mean, max
#
def bw_stats(scans):
	if len(scans) == 0:
		return -1, -1, -1
	scans = scans.flatten('C')
	#scan_bws = (scans[0::2] * BLOCK_SIZE / 2.0**20) / (scans[1::2] / 1000.0)
	scan_bws = (scans[0::2]*1.0) / (scans[1::2] / 1000.0)
	return np.min(scan_bws), np.mean(scan_bws), np.max(scan_bws)

#
# Make log stats
#
def process_log(vals):
	# number of scan types (first two columns)
	seq_scan_cnt, idx_scan_cnt = vals[:,0][0], vals[:,1][0]
	# slice vals into sub-arrays per scan type
	specs, seq_scans, idx_scans = np.hsplit(vals, [2,2+seq_scan_cnt*2])
	# compute bws for each observation (streams * runs)
	seq_scan_stats = bw_stats(seq_scans)
	idx_scan_stats = bw_stats(idx_scans)
	data[seq_scan_cnt, idx_scan_cnt, :3] = seq_scan_stats
	data[seq_scan_cnt, idx_scan_cnt, 3:] = idx_scan_stats

#
# Injest all the *.log files
#
input_filenames = glob.glob('*.log')
for filename in input_filenames:
	vals = np.loadtxt(filename, delimiter=' ')
	process_log(vals)

# seq bandwidth as a function of idx scan count
seq_bws = data[1,:,1]
seq_bws_z = np.polyfit(range(11), seq_bws, 3)
seq_bws_p = np.poly1d(seq_bws_z)
seq_bws_p_eq = "$%.3fx^3%+.3fx^2%+.3fx%+.3f$" % tuple(seq_bws_p.c)

# idx bandwidth as a function of 1 seq scan and N idx scans
idx_bws = data[1,1:,4]
idx_bws_z = np.polyfit(range(1,11), idx_bws, 2)
idx_bws_p = np.poly1d(idx_bws_z)
idx_bws_p_eq = "$%.3fx^2%+.3fx%+.3f$" % tuple(idx_bws_p.c)

fig = plt.figure()
ax = fig.add_subplot(2, 1, 1)
print list(seq_bws)
ax.plot(range(11), seq_bws, 'o-', label='Seq-Scan')
ax.plot(range(11), seq_bws_p(range(11)), 'r--', label=seq_bws_p_eq)
ax.legend()
ax.set_title('Seq-scan Bandwidth: bw_seq(x)')
ax.set_ylabel('MB/s')

ax = fig.add_subplot(2, 1, 2)
ax.plot(range(1,11), idx_bws, 'o-', label='Idx-Scan')
print list(idx_bws)
ax.plot(range(1,11), idx_bws_p(range(1,11)), 'r--', label=idx_bws_p_eq)
ax.legend()
ax.set_title('Average Idx-scan Bandwidth: bw_ix(x)')
ax.set_ylabel('MB/s')
ax.set_xlabel('Concurrent idx-scans')

plt.savefig('bw_seq1_rnd0-10.png')
