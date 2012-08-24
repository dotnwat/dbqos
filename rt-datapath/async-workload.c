#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libaio.h>

/*
 * aio_maxio: max number of outstanding IOs
 * aio_blksize: read size
 */
static int aio_maxio;
static int aio_blksize;

struct iocb_data {
	struct iocb job;
	struct timeval submitted;
	void *buf;
};

static int aio_inflight = 0;

static struct iocb_data **iocb_free;
static int iocb_free_count;
static int alignment = 512;

/*
 * Source we are reading from
 */
static int fd;

/*
 * setup iocb structs.
 *
 *  - n: max number of ios
 */
static int init_iocb(void)
{
	int i;

	/* allocate the pointers */
	iocb_free = malloc(aio_maxio * sizeof(*iocb_free));
	if (!iocb_free)
		return -1;

	/* allocate iocb, buffer, etc... */
	for (i = 0; i < aio_maxio; i++) {

		iocb_free[i] = malloc(sizeof(**iocb_free));
		if (!iocb_free[i])
			return -1;

		if (posix_memalign(&iocb_free[i]->buf, alignment, aio_blksize)) {
			fprintf(stderr, "posix_memalign: failed\n");
			return -1;
		}
		
		printf("assigned: %p - %p\n", iocb_free[i], iocb_free[i]->buf);
	}

	iocb_free_count = aio_maxio;
	return 0;
}

static struct iocb_data *alloc_iocb(void)
{
	if (!iocb_free_count)
		return NULL;
	return iocb_free[--iocb_free_count];
}

static void free_iocb(struct iocb_data *io)
{
	iocb_free[iocb_free_count++] = io;
}

static void read_done(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
	struct iocb_data *iocbd = (struct iocb_data *)iocb;

	if (res2 != 0) {
		fprintf(stderr, "read_done: %s\n", strerror(-res2));
		exit(1);
	}

	if (res != 4096) {
		fprintf(stderr, "read_done: %ld/%d, %s\n", res, 4096, strerror(-res));
		exit(1);
	}

	free_iocb(iocbd);
	aio_inflight--;
}

static int io_wait_run(io_context_t ctx)
{
	struct io_event events[aio_maxio];
	struct io_event *ep;
	int ret, i;

	ret = io_getevents(ctx, 1, aio_maxio, events, NULL);
	if (ret < 1) {
		fprintf(stderr, "io_getevents: %s\n", strerror(-ret));
		exit(1);
	}

	//printf("io_wait_run: events=%d\n", ret);
	for (i = 0; i < ret; i++) {
		ep = events + i;
		read_done(ctx, ep->obj, ep->res, ep->res2);
	}

	return 0;
}

static int run(void)
{
	int ret, n, i;
	io_context_t ctx;

	/*
	 * Initialize libaio state machine
	 */
	memset(&ctx, 0, sizeof(ctx));
	ret = io_queue_init(aio_maxio, &ctx);
	if (ret) {
		fprintf(stderr, "io_queue_init: %s\n", strerror(-ret));
		exit(1);
	}

	/*
	 * Setup iocb contexts
	 */
	if (init_iocb()) {
		perror("init_iocb");
		exit(1);
	}

	while (1) {
		/* submit as many at once as we can */
		n = MIN(aio_maxio - aio_inflight, aio_maxio);
		//printf("min: n=%d, aio_inflight=%d\n", n, aio_inflight);

		if (n > 0) {
			struct iocb *ioq[n];
			struct timeval submitted;

			assert(gettimeofday(&submitted, NULL) == 0);

			for (i = 0; i < n; i++) {
				struct iocb_data *io = alloc_iocb();
				struct iocb *job = (struct iocb *)io;
				assert(io); /* sanity check */

				//io->submitted = submitted;
				printf("%p - %p\n", io, io->buf);
				io_prep_pread(job, fd, io->buf, 4096, 0);
				ioq[i] = job;
			}

			ret = io_submit(ctx, n, ioq);
			if (ret < 0) {
				fprintf(stderr, "io_submit: %s\n", strerror(-ret));
				exit(1);
			}

			aio_inflight += n;
			assert(aio_inflight <= aio_maxio);
			//printf("io_submit: n=%d aio_inflight=%d\n", n, aio_inflight);
		}

		ret = io_wait_run(ctx);
		if (ret < 0) {
			fprintf(stderr, "io_wait_run: %s\n", strerror(-ret));
			exit(1);
		}
	}

	return 0;
}

static void usage(void)
{
	fprintf(stderr, "usage: -s <source> -m <aio_maxio>\n");
}

int main(int argc, char **argv)
{
	char c;
	char *source = NULL;

	while ((c = getopt(argc, argv, "s:m:")) != -1) {
		switch (c) {
		case 's':
			source = strdup(optarg);
			break;
		case 'm':
			aio_maxio = atoi(optarg);
			break;
		default:
			usage();
			exit(1);
		}
	}

	/*
	 * Verify options
	 */
	if (!source) {
		usage();
		exit(1);
	}

	if (aio_maxio < 1 || aio_maxio > 100000) {
		fprintf(stderr, "aio_maxio = %d is crazy!\n", aio_maxio);
		usage();
		exit(1);
	}

	/*
	 * Open what we will be reading from
	 */
	fd = open(source, O_DIRECT|O_RDONLY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	return run();
}
