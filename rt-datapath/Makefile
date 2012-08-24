CC=cc
CFLAGS=-Wall

all: workload
#rnd

workload:%: %.c
	$(CC) $(CFLAGS) -o $@ $< -lpthread

rnd:%: %.c
	$(CC) $(CFLAGS) -o $@ $< -lpthread -laio

clean:
	rm -f workload async-workload rnd
