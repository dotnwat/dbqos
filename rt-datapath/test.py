import matplotlib.pylab as plt
import scikits.statsmodels.tools as sm
import numpy as np

sample = np.random.uniform(-1000, 1000, 5000)
ecdf = sm.tools.ECDF(sample)

x = np.linspace(min(sample), max(sample))
y = ecdf(x)
plt.step(x,y)
plt.show()

#a = array([...]) # your array of numbers
#a = numpy.random.randint(-1000, 1000, size=2000)
#num_bins = 20
#counts, bin_edges = numpy.histogram(a, bins=num_bins, normed=True)
#print counts, counts.sum()
#cdf = numpy.cumsum(counts)
#plt.plot(bin_edges[1:], cdf)
#plt.show()
