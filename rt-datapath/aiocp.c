/*
 * Copyright (c) 2004 Daniel McNeil <daniel@osdl.org>
 *               2004 Open Source Development Lab
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Module: .c
 */

/*
 * Change History:
 *
 *
 * version of copy command using async i/o
 * From:	Stephen Hemminger <shemminger@osdl.org>
 * Modified by Daniel McNeil <daniel@osdl.org> for testing aio.
 *	- added -a alignment
 *	- added -b blksize option 
 *	_ added -s size	option
 *	- added -f open_flag option
 *	- added -w (no write) option (reads from source only)
 *	- added -n (num aio) option 
 *	- added -z (zero dest) opton (writes zeros to dest only)
 *	- added -D delay_ms option
 *  - 2/2004  Marty Ridgeway (mridge@us.ibm.com) Changes to adapt to LTP
 *
 * Copy file by using a async I/O state machine.
 * 1. Start read request
 * 2. When read completes turn it into a write request
 * 3. When write completes decrement counter and free resources
 *
 *
 * Usage: aiocp [-b blksize] -n [num_aio] [-w] [-z] [-s filesize] 
 *		[-f DIRECT|TRUNC|CREAT|SYNC|LARGEFILE] src dest
 */

#define _GNU_SOURCE
//#define DEBUG 1

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/select.h>

#include <libaio.h>

#define AIO_BLKSIZE	(64*1024)
#define AIO_MAXIO	32

static int aio_blksize = AIO_BLKSIZE;
static int aio_maxio = AIO_MAXIO;

static int busy = 0;		// # of I/O's in flight
static int tocopy = 0;		// # of blocks left to copy
static int srcfd;		// source fd
static int dstfd = -1;		// destination file descriptor
static const char *dstname = NULL;
static const char *srcname = NULL;
static int source_open_flag = O_RDONLY;	/* open flags on source file */
static int dest_open_flag = O_WRONLY;	/* open flags on dest file */
static int no_write;			/* do not write */
static int zero;			/* write zero's only */

static int debug;
static int count_io_q_waits;	/* how many time io_queue_wait called */

struct iocb **iocb_free;	/* array of pointers to iocb */
int iocb_free_count;		/* current free count */
int alignment = 512;		/* buffer alignment */

struct timeval delay;		/* delay between i/o */

int init_iocb(int n, int iosize)
{
	void *buf;
	int i;

	if ((iocb_free = malloc(n * sizeof(struct iocb *))) == 0) {
		return -1;
	}

	for (i = 0; i < n; i++) {
		if (!(iocb_free[i] = (struct iocb *) malloc(sizeof(struct iocb))))
			return -1;
		if (posix_memalign(&buf, alignment, iosize))
			return -1;
		if (debug > 1) {
			printf("buf allocated at 0x%p, align:%d\n",
					buf, alignment);
		}
		if (zero) {
			/*
			 * We are writing zero's to dstfd
			 */
			memset(buf, 0, iosize);
		}
		io_prep_pread(iocb_free[i], -1, buf, iosize, 0);
	}
	iocb_free_count = i;
	return 0;
}

struct iocb *alloc_iocb()
{
	if (!iocb_free_count)
		return 0;
	return iocb_free[--iocb_free_count];
}

void free_iocb(struct iocb *io)
{
	iocb_free[iocb_free_count++] = io;
}

/*
 * io_wait_run() - wait for an io_event and then call the callback.
 */
int io_wait_run(io_context_t ctx, struct timespec *to)
{
	struct io_event events[aio_maxio];
	struct io_event *ep;
	int ret, n;

	/*
	 * get up to aio_maxio events at a time.
	 */
	ret = n = io_getevents(ctx, 1, aio_maxio, events, to);

	/*
	 * Call the callback functions for each event.
	 */
	for (ep = events; n-- > 0; ep++) {
		io_callback_t cb = (io_callback_t)ep->data;
		struct iocb *iocb = (struct iocb *)ep->obj;

		cb(ctx, iocb, ep->res, ep->res2);
	}
	return ret;
}

/* Fatal error handler */
static void io_error(const char *func, int rc)
{
	if (rc == -ENOSYS)
		fprintf(stderr, "AIO not in this kernel\n");
	else if (rc < 0)
		fprintf(stderr, "%s: %s\n", func, strerror(-rc));
	else
		fprintf(stderr, "%s: error %d\n", func, rc);

	if (dstfd > 0)
		close(dstfd);
	if (dstname && dest_open_flag & O_CREAT)
		unlink(dstname);
	exit(1);
}

/*
 * Write complete callback.
 * Adjust counts and free resources
 */
static void wr_done(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
	if (res2 != 0) {
		io_error("aio write", res2);
	}
	if (res != iocb->u.c.nbytes) {
		fprintf(stderr, "write missed bytes expect %lu got %ld\n",
			iocb->u.c.nbytes, res2);
		exit(1);
	}
	--tocopy;
	--busy;
	free_iocb(iocb);
	if (debug)
		write(2, "w", 1);
}

/*
 * Read complete callback.
 * Change read iocb into a write iocb and start it.
 */
static void rd_done(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
	/* library needs accessors to look at iocb? */
	int iosize = iocb->u.c.nbytes;
	char *buf = iocb->u.c.buf;
	off_t offset = iocb->u.c.offset;

	if (res2 != 0)
		io_error("aio read", res2);
	if (res != iosize) {
		fprintf(stderr, "read missing bytes expect %lu got %ld\n",
			iocb->u.c.nbytes, res);
		exit(1);
	}


	/* turn read into write */
	if (no_write) {
		--tocopy;
		--busy;
		free_iocb(iocb);
	} else {
		io_prep_pwrite(iocb, dstfd, buf, iosize, offset);
		io_set_callback(iocb, wr_done);
		if (1 != (res = io_submit(ctx, 1, &iocb)))
			io_error("io_submit write", res);
	}
	if (debug)
		write(2, "r", 1);
	if (debug > 1)
		printf("%d", iosize);
}

void usage()
{
	fprintf(stderr,
		"Usage: aiocp [-a align] [-s size] [-b blksize] [-n num_io]"
		" [-f open_flag] SOURCE DEST\n"
		"This copies from SOURCE to DEST using AIO.\n\n"
		"Usage: aiocp [options] -w SOURCE\n"
		"This does sequential AIO reads (no writes).\n\n"
		"Usage: aiocp [options] -z DEST\n"
		"This does sequential AIO writes of zeros.\n");
		
	exit(1);
}

/*
 * Scale value by kilo, mega, or giga.
 */
long long scale_by_kmg(long long value, char scale)
{
	switch (scale) {
	case 'g':
	case 'G':
		value *= 1024;
	case 'm':
	case 'M':
		value *= 1024;
	case 'k':
	case 'K':
		value *= 1024;
		break;
	case '\0':
		break;
	default:
		usage();
		break;
	}
	return value;
}

int main(int argc, char *const *argv)
{
	struct stat st;
	off_t length = 0, offset = 0;
	io_context_t myctx;
	int c;
	extern char *optarg;
	extern int optind, opterr, optopt;

	while ((c = getopt(argc, argv, "a:b:df:n:s:wzD:")) != -1) {
		char *endp;

		switch (c) {
		case 'a':	/* alignment of data buffer */
			alignment = strtol(optarg, &endp, 0);
			alignment = (long)scale_by_kmg((long long)alignment,
							*endp);
			break;
		case 'f':	/* use these open flags */
			if (strcmp(optarg, "LARGEFILE") == 0 ||
			    strcmp(optarg, "O_LARGEFILE") == 0) {
				source_open_flag |= O_LARGEFILE;
				dest_open_flag |= O_LARGEFILE;
			} else if (strcmp(optarg, "TRUNC") == 0 ||
			           strcmp(optarg, "O_TRUNC") == 0) {
				dest_open_flag |= O_TRUNC;
			} else if (strcmp(optarg, "SYNC") == 0 ||
				   strcmp(optarg, "O_SYNC") == 0) {
				dest_open_flag |= O_SYNC;
			} else if (strcmp(optarg, "DIRECT") == 0 ||
				   strcmp(optarg, "O_DIRECT") == 0) {
				source_open_flag |= O_DIRECT;
				dest_open_flag |= O_DIRECT;
			} else if (strncmp(optarg, "CREAT", 5) == 0 ||
				   strncmp(optarg, "O_CREAT", 5) == 0) {
				dest_open_flag |= O_CREAT;
			}
			break;
		case 'd':
			debug++;
			break;
		case 'D':
			delay.tv_usec = atoi(optarg);
			break;
		case 'b':	/* block size */
			aio_blksize = strtol(optarg, &endp, 0);
			aio_blksize = (long)scale_by_kmg((long long)aio_blksize, *endp);
			break;

		case 'n':	/* num io */
			aio_maxio = strtol(optarg, &endp, 0);
			break;
		case 's':	/* size to transfer */
			length = strtoll(optarg, &endp, 0);
			length = scale_by_kmg(length, *endp);
			break;
		case 'w':	/* no write */
			no_write = 1;
			break;
		case 'z':	/* write zero's */
			zero = 1;
			break;

		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	
#ifndef DEBUG
	if (argc < 1) {
		usage();
	}
#else
    source_open_flag |= O_DIRECT;
    dest_open_flag |= O_DIRECT;
    aio_blksize = 1;
    aio_maxio=1;
    srcname = "junkdata";
    dstname = "ff2";
#endif
	if (!zero) {
#ifndef DEBUG
	       	if ((srcfd = open(srcname = *argv, source_open_flag)) < 0) {
#else
                if ((srcfd = open(srcname, source_open_flag)) < 0) {
#endif
			perror(srcname);
			exit(1);
		}
		argv++;
		argc--;
		length = 1073741824;
#if 0
		if (fstat(srcfd, &st) < 0) {
			perror("fstat");
			exit(1);
		}
		if (length == 0)
			length = st.st_size;
#endif
	}

	if (!no_write) {
		/*
		 * We are either copying or writing zeros to dstname
		 */
#ifndef DEBUG
		if (argc < 1) {
			usage();
		}
		if ((dstfd = open(dstname = *argv, dest_open_flag, 0666)) < 0) {
#else
            if ((dstfd = open(dstname, dest_open_flag, 0666)) < 0) {
#endif
			perror(dstname);
			exit(1);
		}
		if (zero) {
			/*
			 * get size of dest, if we are zeroing it.
			 * TODO: handle devices.
			 */
			if (fstat(dstfd, &st) < 0) {
				perror("fstat");
				exit(1);
			}
			if (length == 0)
				length = st.st_size;
		}
	}

	/* initialize state machine */
	memset(&myctx, 0, sizeof(myctx));
	io_queue_init(aio_maxio, &myctx);
	tocopy = howmany(length, aio_blksize);

	if (init_iocb(aio_maxio, aio_blksize) < 0) {
		fprintf(stderr, "Error allocating the i/o buffers\n");
		exit(1);
	}

	while (tocopy > 0) {
		int i, rc;
		/* Submit as many reads as once as possible upto aio_maxio */
		int n = MIN(MIN(aio_maxio - busy, aio_maxio),
				howmany(length - offset, aio_blksize));
		if (n > 0) {
			struct iocb *ioq[n];

			for (i = 0; i < n; i++) {
				struct iocb *io = alloc_iocb();
				int iosize = MIN(length - offset, aio_blksize);

				if (zero) {
					/*
					 * We are writing zero's to dstfd
					 */
					io_prep_pwrite(io, dstfd, io->u.c.buf,
							iosize, offset);
					io_set_callback(io, wr_done);
				} else {
					io_prep_pread(io, srcfd, io->u.c.buf,
							iosize, offset);
					io_set_callback(io, rd_done);
				}
				ioq[i] = io;
				offset += iosize;
			}

			rc = io_submit(myctx, n, ioq);
			if (rc < 0)
				io_error("io_submit", rc);

			busy += n;
			if (debug > 1)
				printf("io_submit(%d) busy:%d\n", n, busy);
			if (delay.tv_usec) {
				struct timeval t = delay;
				(void)select(0,0,0,0,&t);
			}
		}

		/*
		 * We have submitted all the i/o requests. Wait for at least one to complete
		 * and call the callbacks.
		 */
		count_io_q_waits++;
		rc = io_wait_run(myctx, 0);
		if (rc < 0)
			io_error("io_wait_run", rc);

		if (debug > 1) {
			printf("io_wait_run: rc == %d\n", rc);
			printf("busy:%d aio_maxio:%d tocopy:%d\n",
					busy, aio_maxio, tocopy);
		}
	}

	if (srcfd != -1)
		close(srcfd);
	if (dstfd != -1)
		close(dstfd);
	exit(0);
}

/* 
 * Results look like:
 * [alanm@toolbox ~/MOT3]$ ../taio -d kernel-source-2.4.8-0.4g.ppc.rpm abc
 * rrrrrrrrrrrrrrrwwwrwrrwwrrwrwwrrwrwrwwrrwrwrrrrwwrwwwrrwrrrwwwwwwwwwwwwwwwww
 * rrrrrrrrrrrrrrwwwrrwrwrwrwrrwwwwwwwwwwwwwwrrrrrrrrrrrrrrrrrrwwwwrwrwwrwrwrwr
 * wrrrrrrrwwwwwwwwwwwwwrrrwrrrwrrwrwwwwwwwwwwrrrrwwrwrrrrrrrrrrrwwwwwwwwwwwrww
 * wwwrrrrrrrrwwrrrwwrwrwrwwwrrrrrrrwwwrrwwwrrwrwwwwwwwwrrrrrrrwwwrrrrrrrwwwwww
 * wwwwwwwrwrrrrrrrrwrrwrrwrrwrwrrrwrrrwrrrwrwwwwwwwwwwwwwwwwwwrrrwwwrrrrrrrrrr
 * rrwrrrrrrwrrwwwwwwwwwwwwwwwwrwwwrrwrwwrrrrrrrrrrrrrrrrrrrwwwwwwwwwwwwwwwwwww
 * rrrrrwrrwrwrwrrwrrrwwwwwwwwrrrrwrrrwrwwrwrrrwrrwrrrrwwwwwwwrwrwwwwrwwrrrwrrr
 * rrrwwwwwwwrrrrwwrrrrrrrrrrrrwrwrrrrwwwwwwwwwwwwwwrwrrrrwwwwrwrrrrwrwwwrrrwww
 * rwwrrrrrrrwrrrrrrrrrrrrwwwwrrrwwwrwrrwwwwwwwwwwwwwwwwwwwwwrrrrrrrwwwwwwwrw
 */
