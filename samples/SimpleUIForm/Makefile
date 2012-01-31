CFLAGS = -Wall -g -DGDA_DISABLE_DEPRECATED `pkg-config --cflags libgda-ui-5.0`
LDFLAGS = `pkg-config --libs libgda-ui-5.0`

all: example-rw example-ro

example-rw: example.c
	$(CC) -o example-rw example.c $(CFLAGS) -DWRITABLE $(LDFLAGS)

example-ro: example.c
	$(CC) -o example-ro example.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f example-r*
