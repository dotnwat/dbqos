/*
 *
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <libaio.h>

struct iocb_context {
	struct timeval submitted;
};

struct workload {
	int aio_blksize;	/* size of op */
	int aio_maxio;		/* max # inflight */
	int aio_inflight;	/* active IOs */

	/* file */
	char *filename;
	long long size;
	long long blocks;	/* (filesize-aio_blksize) / alignment */
	int fd;

	struct iocb **iocb_free;
	int iocb_free_count;
	int alignment;

	io_context_t ctx;
};

#define USEC_PER_SEC (1000000)

/*
 * Generate a random offset
 */
static long long rnd_offset(struct workload *w)
{
	long long block;

	block = ((double)(w->blocks-1)) * (((double)rand()) / ((double)RAND_MAX));
	assert(((block * w->aio_blksize) % 4096) == 0);
	return block * w->aio_blksize;
}

static struct iocb *alloc_iocb(struct workload *w)
{
	if (!w->iocb_free_count)
		return NULL;
	return w->iocb_free[--w->iocb_free_count];
}

static void free_iocb(struct workload *w, struct iocb *io)
{
	w->iocb_free[w->iocb_free_count++] = io;
}

static int init_iocb(struct workload *w)
{
	int i, ret;
	void *buf;

	w->iocb_free = malloc(w->aio_maxio * sizeof(*w->iocb_free));
	if (!w->iocb_free) {
		perror("malloc");
		return -1;
	}

	for (i = 0; i < w->aio_maxio; i++) {
		w->iocb_free[i] = malloc(sizeof(**w->iocb_free));
		if (!w->iocb_free[i]) {
			perror("malloc");
			return -1;
		}
		ret = posix_memalign(&buf, w->alignment, w->aio_blksize);
		if (ret) {
			fprintf(stderr, "posix_memalign: %s\n", strerror(-ret));
			return ret;
		}
		/* this is just used to save a pointer to buf */
		io_prep_pread(w->iocb_free[i], -1, buf, w->aio_blksize, 0);

		/* stash some context in iocb->data */
		w->iocb_free[i]->data = malloc(sizeof(struct iocb_context));
		if (!w->iocb_free[i]->data) {
			perror("malloc");
			return -1;
		}
	}

	w->iocb_free_count = i;
	return 0;
}

static int init_workload(struct workload *w, char *filename, long long size,
		int aio_maxio, int aio_blksize)
{
	int fd, ret;

	fd = open(filename, O_DIRECT|O_RDONLY);
	if (fd < 0) {
		perror("open");
		return fd;
	}

	w->fd = fd;
	w->filename = filename;
	w->aio_maxio = aio_maxio;
	w->aio_blksize = aio_blksize;
	w->aio_inflight = 0;
	w->alignment = 512;
	w->size = size;
	w->blocks = (w->size - w->aio_blksize) / w->aio_blksize;
	memset(&w->ctx, 0, sizeof(w->ctx));

	ret = io_queue_init(w->aio_maxio, &w->ctx);
	if (ret) {
		fprintf(stderr, "io_queue_init: %s\n", strerror(-ret));
		return ret;
	}

	ret = init_iocb(w);
	if (ret)
		return ret;

	return 0;
}

/*
 * Convert timeval to microseconds
 */
static unsigned long long timeval_to_us(struct timeval *a)
{
	unsigned long long ts;

	ts = a->tv_sec * USEC_PER_SEC;
	ts += a->tv_usec;

	return ts;
}

/*
 * Subtract timevals: a - b (usecs)
 */
static unsigned long long timeval_diff(struct timeval *a, struct timeval *b)
{
	return timeval_to_us(a) - timeval_to_us(b);
}

static void rd_done(struct workload *w, struct timeval *completed,
		struct iocb *iocb, void *data, long res, long res2)
{
	struct iocb_context *iocb_ctx = data;
	unsigned long long usdiff;

	if (res2) {
		fprintf(stderr, "rd_done: res2=%ld, %s\n", res2, strerror(-res2));
		exit(1);
	}

	if (res != w->aio_blksize) {
		fprintf(stderr, "read missing bytes! %s\n", strerror(-res));
		exit(1);
	}

	usdiff = timeval_diff(completed, &iocb_ctx->submitted);
	//printf("%llu\n", usdiff);

	w->aio_inflight--;
	free_iocb(w, iocb);
}

static int io_wait_run(struct workload *w)
{
	struct io_event events[w->aio_maxio];
	struct io_event *ep;
	struct timeval completed;
	int ret, i;

	ret = io_getevents(w->ctx, 1, w->aio_maxio, events, NULL);
	if (ret < 1) {
		fprintf(stderr, "io_getevents: %s\n", strerror(-ret));
		return ret;
	}

	assert(gettimeofday(&completed, NULL) == 0);

	for (i = 0; i < ret; i++) {
		ep = events + i;
		rd_done(w, &completed, ep->obj, ep->data, ep->res, ep->res2);
	}

	return 0;
}

static int run_workload(struct workload *w)
{
	int i, n, ret;
	struct iocb *io;
	struct iocb_context *iocb_ctx;
	struct timeval submitted;
	void *data;
	long long offset = 0;

	while (1) {
		n = MIN(w->aio_maxio - w->aio_inflight, w->aio_maxio);
		if (n > 0) {
			struct iocb *ioq[n];
			
			for (i = 0; i < n; i++) {
				io = alloc_iocb(w);
				assert(io); /* sanity */
				data = io->data;
				io_prep_pread(io, w->fd, io->u.c.buf, w->aio_blksize, rnd_offset(w));
				//io_prep_pread(io, w->fd, io->u.c.buf, w->aio_blksize, offset);
				offset += 4096;
				io->data = data;
				ioq[i] = io;
			}

			/* all these dudes get the same submit time */
			assert(gettimeofday(&submitted, NULL) == 0);
			for (i = 0; i < n; i++) {
				iocb_ctx = ioq[i]->data;
				iocb_ctx->submitted = submitted;
			}

			ret = io_submit(w->ctx, n, ioq);
			if (ret < n) {
				fprintf(stderr, "io_submit: %s\n", strerror(-ret));
				return -1;
			}

			w->aio_inflight += n;
		}

		ret = io_wait_run(w);
		if (ret)
			return -1;
	}

	return 0;
}

static void usage(void)
{
	fprintf(stderr, "usage: -s <source> -m <aio_maxio> -b <aio_blksize> -l <size>\n");
	exit(1);
}

int main(int argc, char **argv)
{
	struct workload w;
	char *source = NULL;
	int aio_maxio = -1;
	int aio_blksize = -1;
	long long size = -1;
	int ret;
	char c;

	while ((c = getopt(argc, argv, "s:m:b:l:")) != -1) {
		switch (c) {
		case 's':
			source = strdup(optarg);
			break;
		case 'm':
			aio_maxio = atoi(optarg);
			break;
		case 'b':
			aio_blksize = atoi(optarg);
			break;
		case 'l':
			size = atoll(optarg);
			break;
		default:
			usage();
		}
	}

	if (!source)
		usage();

	if (aio_maxio < 1 || aio_maxio > 70000) {
		fprintf(stderr, "aio_maxio = %d is crazy!\n", aio_maxio);
		usage();
	}

	if (aio_blksize < 1 || aio_blksize > (1<<20)) {
		fprintf(stderr, "aio_blksize = %d is crazy!\n", aio_blksize);
		usage();
	}

	if (size < 1)
		usage();

	ret = init_workload(&w, source, size, aio_maxio, aio_blksize);
	if (ret)
		return ret;

	ret = run_workload(&w);
	if (ret)
		return ret;

	return 0;
}
