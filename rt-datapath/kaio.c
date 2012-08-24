/*
 * Advanced Linux IO
 * Linux KAIO
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "kaio.h"

#undef BUFSIZ
#ifndef BUFSIZ
#define BUFSIZ		(1024 * 1024 * 128)
#endif

/* file names */
static char *files[] = {
    "test/slo.txt"
    ,"test/oer.txt"
    ,"test/rse.txt"
    ,"test/ufver.txt"
};

/* file descriptors */
static int *fds;
static char g_buffer[BUFSIZ];

static void open_files(void)
{
	size_t n_files = sizeof(files) / sizeof(files[0]);
	size_t i;

	fds = (int *) malloc(n_files * sizeof(int));
	if (fds == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < n_files; i++) {
		fds[i] = open(files[i], O_CREAT | O_WRONLY | O_TRUNC | O_NONBLOCK, 0666);
		if (fds[i] < 0) {
			perror("open");
			exit(EXIT_FAILURE);
		}
	}
}

static void close_files(void)
{
	size_t n_files = sizeof(files) / sizeof(files[0]);
	size_t i;

	for (i = 0; i < n_files; i++)
		close(fds[i]);

	free(fds);
}

/*
 * init buffer with random data
 */
static void init_buffer(void)
{
	size_t i;

	srand(time(NULL));

	for (i = 0; i < BUFSIZ; i++)
		g_buffer[i] = 'a' + (char) rand() % 20;
}

/*
 * init iocb structure
 *
 * opcode may be IOCB_CMD_PWRITE, IOCB_CMD_PREAD
 */
static inline void init_iocb(struct iocb *iocb, int fd,
		void const *buf, size_t nbytes, off_t offset, int opcode)
{
	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = fd;
	iocb->aio_lio_opcode = opcode;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (u_int64_t) buf;
	iocb->aio_nbytes = nbytes;
	iocb->aio_offset = offset;
	iocb->aio_flags = 0;
}

/*
 * wait for asynchronous I/O operations
 * (eventfd or io_getevents)
 */
static void wait_aio(aio_context_t ctx, int nops)
{
	struct io_event *events;

	events = (struct io_event *) malloc(nops * sizeof(struct io_event));
	if (events == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

    if (io_getevents(ctx, nops, nops, events, NULL) < 0)
    {
        perror("io_getevents");
        exit(EXIT_FAILURE);
    }
}

/*
 * write data asynchronously (using io_setup(2), io_sumbit(2),
 * 	io_getevents(2), io_destroy(2))
 */

static void do_io_async(void)
{
	size_t n_files = sizeof(files) / sizeof(files[0]);
	size_t i;
	aio_context_t ctx = 0;
	struct iocb *iocb;
	struct iocb **piocb;
	int n_aio_ops = 0;

	/* allocate iocb and piocb */
	iocb = (struct iocb *) malloc(n_files * sizeof(*iocb));
	if (iocb == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	piocb = (struct iocb **) malloc(n_files * sizeof(*piocb));
	if (iocb == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	/* initialiaze iocb and piocb */
	for (i = 0; i < n_files; i++) {
		init_iocb(&iocb[i], fds[i], g_buffer, BUFSIZ, 0, IOCB_CMD_PWRITE);
		iocb[i].aio_data = (u_int64_t) i + 1;
		piocb[i] = &iocb[i];
	}

	/* setup aio context */
	if (io_setup(n_files, &ctx) != 0) {
		perror("io_setup");
		exit(EXIT_FAILURE);
	}

	/* submit aio */
	n_aio_ops = io_submit(ctx, n_files, piocb);
	if (n_aio_ops < 0) {
		printf("n_aio_ops = %d\n", n_aio_ops);
		printf("errno = %d\n", errno);
		perror("io_submit");
		exit(EXIT_FAILURE);
	}
    printf("Submitted %d ops\n", n_aio_ops);

	/* wait for completion */
	wait_aio(ctx, n_files);

	/* destroy aio context */
	io_destroy(ctx);
}

int main(void)
{
	open_files();
	init_buffer();

	do_io_async();

	close_files();

	return 0;
}
