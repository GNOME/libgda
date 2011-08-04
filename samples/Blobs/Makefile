CFLAGS = -Wall -g -DGDA_DISABLE_DEPRECATED `pkg-config --cflags libgda-5.0`
LDFLAGS = `pkg-config --libs libgda-5.0`

all: blobtest

blobtest: blobtest.c
	$(CC) -o blobtest blobtest.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f blobtest
	rm -f fetched_*
