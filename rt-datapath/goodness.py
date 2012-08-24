import string
import itertools
import copy
import numpy as np
import scikits.statsmodels.tools as sm
import scipy.stats
import sys

import matplotlib
matplotlib.use('Agg')
from matplotlib.transforms import TransformedBbox, Affine2D
import matplotlib.pyplot as plt

PAD_INCHES = 0.1

def tight_layout(fig, pad_inches=PAD_INCHES, h_pad_inches=None, w_pad_inches=None):
    """Adjust subplot parameters to give specified padding.
    
    Parameters
    ----------
    pad_inches : float
        minimum padding between the figure edge and the edges of subplots.
    h_pad_inches, w_pad_inches : float
        minimum padding (height/width) between edges of adjacent subplots.
        Defaults to `pad_inches`.
    """
    if h_pad_inches is None:
        h_pad_inches = pad_inches
    if w_pad_inches is None:
        w_pad_inches = pad_inches
#    fig = plt.gcf()
    tight_borders(fig, pad_inches=pad_inches)
    # NOTE: border padding affects subplot spacing; tighten border first
    tight_subplot_spacing(fig, h_pad_inches, w_pad_inches)

def tight_borders(fig, pad_inches=PAD_INCHES):
    """Stretch subplot boundaries to figure edges plus padding."""
    # call draw to update the renderer and get accurate bboxes.
    fig.canvas.draw()
    bbox_original = fig.bbox_inches
    bbox_tight = _get_tightbbox(fig, pad_inches)
    
    # figure dimensions ordered like bbox.extents: x0, y0, x1, y1
    lengths = np.array([bbox_original.width, bbox_original.height,
                        bbox_original.width, bbox_original.height])
    whitespace = (bbox_tight.extents - bbox_original.extents) / lengths
    
    # border padding ordered like bbox.extents: x0, y0, x1, y1
    current_borders = np.array([fig.subplotpars.left, fig.subplotpars.bottom,
                                fig.subplotpars.right, fig.subplotpars.top])
    
    left, bottom, right, top = current_borders - whitespace
    fig.subplots_adjust(bottom=bottom, top=top, left=left, right=right)

def _get_tightbbox(fig, pad_inches):
    renderer = fig.canvas.get_renderer()
    bbox_inches = fig.get_tightbbox(renderer)
    return bbox_inches.padded(pad_inches)

def tight_subplot_spacing(fig, h_pad_inches, w_pad_inches):
    """Stretch subplots so adjacent subplots are separated by given padding."""
    # Zero hspace and wspace to make it easier to calculate the spacing.
    fig.subplots_adjust(hspace=0, wspace=0)
    fig.canvas.draw()
    
    figbox = fig.bbox_inches
    ax_bottom, ax_top, ax_left, ax_right = _get_grid_boundaries(fig)
    nrows, ncols = ax_bottom.shape
    
    subplots_height = fig.subplotpars.top - fig.subplotpars.bottom
    if nrows > 1:
        h_overlap_inches = ax_top[1:] - ax_bottom[:-1]
        hspace_inches = h_overlap_inches.max() + h_pad_inches
        hspace_fig_frac = hspace_inches / figbox.height
        hspace = _fig_frac_to_cell_frac(hspace_fig_frac, subplots_height, nrows)
        fig.subplots_adjust(hspace=hspace)
    
    subplots_width = fig.subplotpars.right - fig.subplotpars.left
    if ncols > 1:
        w_overlap_inches = ax_right[:,:-1] - ax_left[:,1:]
        wspace_inches = w_overlap_inches.max() + w_pad_inches
        wspace_fig_frac = wspace_inches / figbox.width
        wspace = _fig_frac_to_cell_frac(wspace_fig_frac, subplots_width, ncols)
        fig.subplots_adjust(wspace=wspace)

def _get_grid_boundaries(fig):
    """Return grid boundaries for bboxes of subplots
    
    Returns
    -------
    ax_bottom, ax_top, ax_left, ax_right : array
        bbox cell-boundaries of subplot grid. If a subplot spans cells, the grid
        boundaries cutting through that subplot will be masked.
    """
    nrows, ncols, n = fig.axes[0].get_geometry()
    # Initialize boundaries as masked arrays; in the future, support subplots 
    # that span multiple rows/columns, which would have masked values for grid 
    # boundaries that cut through the subplot.
    ax_bottom, ax_top, ax_left, ax_right = [np.ma.masked_all((nrows, ncols))
                                            for n in range(4)]
    renderer = fig.canvas.get_renderer()
    px2inches_trans = Affine2D().scale(1./fig.dpi)
    for ax in fig.axes:
        ax_bbox = ax.get_tightbbox(renderer)
        x0, y0, x1, y1 = TransformedBbox(ax_bbox, px2inches_trans).extents
        if not hasattr(ax, 'get_geometry'):
			continue
        nrows, ncols, n = ax.get_geometry()
        # subplot number starts at 1, matrix index starts at 0
        i = n - 1
        ax_bottom.flat[i] = y0
        ax_top.flat[i] = y1
        ax_left.flat[i] = x0
        ax_right.flat[i] = x1
    return ax_bottom, ax_top, ax_left, ax_right


def _fig_frac_to_cell_frac(fig_frac, subplots_frac, num_cells):
    """Return fraction of cell (row/column) from a given fraction of the figure
    
    Parameters
    ----------
    fig_frac : float
        length given as a fraction of figure height or width
    subplots_frac : float
        fraction of figure (height or width) occupied by subplots
    num_cells : int
        number of rows or columns.
    """
    # This function is reverse engineered from the calculation of `sepH` and 
    # `sepW` in  `GridSpecBase.get_grid_positions`.
    return (fig_frac * num_cells) / (subplots_frac - fig_frac*(num_cells-1))

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
# Load the best effort performance model
#
best_effort_p = np.load(sys.argv[1])

fig = plt.figure()
ax1 = fig.add_subplot(111)
ax1.set_title("Best Effort Seq vs Rnd IOPS")
ax1.set_ylabel("IOPS (4K)")
ax1.set_xlabel("Concurrent Index-Scan")
#ax1.set_yscale('log')
ax1.grid()
#ax1.plot(range(20), best_effort_p[1,1:,1], 'ro-', label='Seq-Scan')
ax1.plot(range(21), best_effort_p[0,:,4], 'bo-', label='Idx-Scan (0 Seq)')
ax1.plot(range(21), best_effort_p[1,:,4], 'go-', label='Idx-Scan (1 Seq)')
ax1.plot(range(21), map(lambda (x,y): x*y, zip(range(21), best_effort_p[0,:,4])))
ax1.legend()
print best_effort_p[1,:,1]
print best_effort_p[0,:,4]
print best_effort_p[1,:,4]

fig.savefig('out4.png')
#
sys.exit(1);

#
#
#
def eval_goodness(workload, perf_model):
	goodness = []
	for qs, qi in two_subset_partitions(workload):
		iops_model = perf_model[min(1, len(qs)), len(qi)]
		goodness_val = ac_goodness(qs, qi, iops_model)
		goodness.append(goodness_val)
	return np.array(goodness)

#
#
#
def graph_goodness_ecdf(ax, goodness, workload_name):
	ecdf = sm.tools.ECDF(goodness)
	x = np.linspace(min(goodness), max(goodness))
	y = ecdf(x)
	ax.step(x, y)
	ax.set_title("AC Goodness CDF: Workload %s" % (workload_name,))
	ax.set_ylabel('Probability')
	ax.set_xlabel('AC Goodness')
	ax.grid()

#
#
#
def graph_workload_dist(ax, workload_queries):
	x = range(1, len(workload_queries)+1)
	y_blocks, y_secs = zip(*workload_queries)
	ax.plot(x, y_blocks, '-bo')
	ax.set_xlabel('Query')
	ax.set_ylabel('Block Read (4K)', color='b')
	for t1 in ax.get_yticklabels():
		t1.set_color('b')
	ax2 = ax.twinx()
	ax2.plot(x, y_secs, '-ro')
	ax2.set_ylabel('Latency (sec)', color='r')
	for t1 in ax2.get_yticklabels():
		t1.set_color('r')

stats_min = []
stats_25 = []
stats_50 = []
stats_75 = []
stats_max = []
stats_mean = []

#
#
#
def create_goodness_report(workload, perf_model):
	workload_queries = workload["queries"]
	workload_name = workload["name"]
	goodness = eval_goodness(workload_queries, perf_model)

	#fig, (ax1, ax2) = plt.subplots(nrows=2, ncols=1)
	fig = plt.figure()

	# plot the workload
	ax1 = fig.add_subplot(211)
	ax1.set_title('Workload Setup: %s' % (workload_name,))
	graph_workload_dist(ax1, workload_queries)

	# plot the goodness ecdf
	ax2 = fig.add_subplot(212)
	graph_goodness_ecdf(ax2, goodness, workload_name)

	tight_layout(fig)
	fig.savefig("%s.png" % (workload_name,))

	# compute some stats: would like to get this info from ECDF?
	goodness = np.sort(goodness)
	g_min = np.min(goodness)
	g_25 = scipy.stats.scoreatpercentile(goodness, 25)
	g_50 = scipy.stats.scoreatpercentile(goodness, 50)
	g_75 = scipy.stats.scoreatpercentile(goodness, 75)
	g_max = np.max(goodness)
	g_mean = np.mean(goodness)
	print "%s,%d,%d,%d,%d,%d,%d" % (workload_name, g_min,
			g_25, g_50, g_75, g_max, g_mean)
	stats_min.append(g_min)
	stats_25.append(g_25)
	stats_50.append(g_50)
	stats_75.append(g_75)
	stats_max.append(g_max)
	stats_mean.append(g_mean)

#
# Workload
#  - (# 4K Blocks, Seconds)
# 
#workload = {
#	"name": "S.1",
#	"queries": (
#		(262144, 60), # 1GB, 1 min
#		(262144, 50), # 1GB, 50s
#		(262144, 40), # 1GB, 40s
#		(262144, 30), # 1GB, 30s
#		(262144, 20), # 1GB, 20s
#		(262144, 10), # 1GB, 10s
#		(262144,  5), # 1GB, 9s
#		(262144,  3), # 1GB, 8s
#		(262144,  2), # 1GB, 7s
#		(262144,  1), # 1GB, 1s
#	),
#}
#create_goodness_report(workload, best_effort_p)

# build some workloads!
big_blocks = 262144 # 1 GB
small_blocks = 3 # 3 blocks
slow_lat = [10, 30, 60]
fast_lat = [1, 5, 10]
qs = range(10+1)

def run_workload(big, small, big_lat, small_lat):
	queries = [(big_blocks, big_lat)]*big
	queries += [(small_blocks, small_lat)]*small
	workload = {
		"name": "big_%d-%d__small_%d-%d" % (big, big_lat, small, small_lat),
		"queries": tuple(queries),
	}
	create_goodness_report(workload, best_effort_p)

tworkload = {
	"name": "Example",
	"queries": (
		(262144, 90),
		(262144, 80),
		(262144, 70),
		(262144, 60),
		(262144, 50),
		(262144, 40),
		(262144, 30),
		(262144, 20),
		(262144, 10),
		(3, 10)
	)
}
create_goodness_report(tworkload, best_effort_p)
sys.exit(1)


# we divide the 10 queries into big and small groups
# and then in each group we look for different combinations
# of latencies
x = 0
for big, small in zip(qs, reversed(qs)):
	for slow in slow_lat:
		for fast in fast_lat:
			run_workload(big, small, fast, slow)
			run_workload(big, small, slow, fast)

def get_6num_summary(vals):
	return (
		np.min(vals),
		scipy.stats.scoreatpercentile(vals, 25),
		scipy.stats.scoreatpercentile(vals, 50),
		scipy.stats.scoreatpercentile(vals, 75),
		np.max(vals),
		np.mean(vals)
	)
#
#
#
print "min", get_6num_summary(stats_min)
print "25%", get_6num_summary(stats_25)
print "50%", get_6num_summary(stats_50)
print "75%", get_6num_summary(stats_75)
print "max", get_6num_summary(stats_max)
print "mean", get_6num_summary(stats_mean)
