import sys
import numpy as np

#
# Serialize performance model into format for loading
# into CPLEX optimizer.
#
# Format:
#  line1: t_S
#  line2: t_I
#  line3: t_Is
#
if __name__ == '__main__':
	perf_model = np.load(sys.argv[1])

	# t_S:
	#  - seq stream iops
	#  - function of # rnd streams
	line = ""
	for iops in perf_model[1,:,1]:
		line += "%.3f " % (iops,)
	line = line[:-1] + "\n"

    # t_I:
	#  - rnd stream iops
	#  - function of 0 seq streams and # rnd streams
	for iops in perf_model[0,:,4]:
		line += "%.3f " % (iops,)
	line = line[:-1] + "\n"

    # t_Is:
	#  - rnd stream iops
	#  - function of 1 seq stream and # rnd streams
	for iops in perf_model[1,:,4]:
		line += "%.3f " % (iops,)
	line = line[:-1] + "\n"

	sys.stdout.write(line)
