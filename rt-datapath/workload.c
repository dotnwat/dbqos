#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

#define READ_SIZE (4096)

/* some reasonble bounds */
#define MAX_THREADS 100
#define MAX_NAME 256

static int num_threads;
static pthread_t threads[MAX_THREADS];
static volatile int start_obs = 0;
static volatile int stop = 0;

struct thread_info {
	char filename[MAX_NAME];
	unsigned int blocks_read;
	int random_workload;
	struct timeval start;
	struct timeval finish;
};

static struct thread_info tinfo[MAX_THREADS];

#define USEC_PER_SEC (1000000)
#define USEC_PER_MSEC (1000)

/*
 * do a random seek
 */
static void do_random_seek(int fd, int num_blocks)
{
	off_t block;
	off_t offset;

	block = 1 + (int)((float)num_blocks * (rand() / (RAND_MAX + 1.0)));
	if (block >= num_blocks)
		block = num_blocks - 1;

	offset = block * READ_SIZE;
	assert(lseek(fd, offset, SEEK_SET) == offset);
}

/*
 * Generate a sequential workload
 */
static void *workload(void *arg)
{
	struct thread_info *info = arg;
	char buf[READ_SIZE];
	int fd, local_started_obs = 0;
	struct stat st;
	int num_blocks;

	fd = open(info->filename, O_RDONLY);
	if (fd < 0) {
		perror(info->filename);
		pthread_exit(NULL);
	}

	if (fstat(fd, &st)) {
		perror(info->filename);
		exit(1);
	}

	num_blocks = st.st_size / READ_SIZE;

	info->blocks_read = 0;

	while (!stop) {
		if (start_obs && !local_started_obs) {
			local_started_obs = 1;
			assert(gettimeofday(&info->start, NULL) == 0);
			info->blocks_read = 0;
		}

		if (info->random_workload)
			do_random_seek(fd, num_blocks);

		assert(read(fd, buf, READ_SIZE) == READ_SIZE);
		info->blocks_read++;
	}

	assert(gettimeofday(&info->finish, NULL) == 0);

	close(fd);
	pthread_exit(NULL);
}

/*
 * Convert timeval to milliseconds
 */
static unsigned long long timeval_to_ms(struct timeval *a)
{
	unsigned long long ts;

	ts = a->tv_sec * USEC_PER_SEC;
	ts += a->tv_usec;
	ts /= USEC_PER_MSEC;

	return ts;
}

/*
 * Subtract timevals: a - b
 */
static unsigned long long timeval_diff(struct timeval *a, struct timeval *b)
{
	return timeval_to_ms(a) - timeval_to_ms(b);
}

static void usage(void)
{
	fprintf(stderr, "usage: -s <num seq scans> -x <num idx scans> -b <filename base>\n");
}

int main(int argc, char **argv)
{
	char c;
	int idx_scans = -1;
	int seq_scans = -1;
	char *filename_base = NULL;
	int i;

	while ((c = getopt(argc, argv, "s:x:b:")) != -1) {
		switch  (c) {
			case 'x':
				idx_scans = atoi(optarg);
				break;
			case 's':
				seq_scans = atoi(optarg);
				break;
			case 'b':
				filename_base = strdup(optarg);
				break;
			default:
				usage();
				exit(1);
		}
	}

	if (seq_scans < 0 || idx_scans < 0 || !filename_base) {
		usage();
		exit(1);
	}

	num_threads = seq_scans + idx_scans;
	if (num_threads > MAX_THREADS) {
		fprintf(stderr, "Too many threads! MAX_THREADS=%d\n", MAX_THREADS);
		exit(1);
	}

	/* start the sequential scans */
	for (i = 0; i < seq_scans; i++) {
		sprintf(tinfo[i].filename, "%s.seq.%d.dat", filename_base, i);
		tinfo[i].random_workload = 0;
		assert(pthread_create(threads+i, NULL, workload, &tinfo[i]) == 0);
	}

	/* start the rest: index scans */
	for (; i < num_threads; i++) {
		sprintf(tinfo[i].filename, "%s.rnd.%d.dat", filename_base, i-seq_scans);
		tinfo[i].random_workload = 1;
		assert(pthread_create(threads+i, NULL, workload, &tinfo[i]) == 0);
	}

	/* wait for threads to reach a stable state */
	assert(sleep(10) == 0);
	start_obs = 1;

	/* run experiment for 30 seconds */
	assert(sleep(30) == 0);
	stop = 1;

	/* wait on threads */
	for (i = 0; i < num_threads; i++) {
		assert(pthread_join(threads[i], NULL) == 0);
	}

	/* output the data! */
	printf("%d %d", seq_scans, idx_scans);
	for (i = 0; i < num_threads; i++) {
		struct timeval *s = &tinfo[i].start;
		struct timeval *f = &tinfo[i].finish;
		printf(" %u %llu", tinfo[i].blocks_read, timeval_diff(f, s));
	}
	printf("\n");

	return 0;
}
