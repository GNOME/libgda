CFLAGS = -Wall -g -DGDA_DISABLE_DEPRECATED `pkg-config --cflags libgda-5.0`
LDFLAGS = `pkg-config --libs libgda-5.0`

all: example example_cnc

example: example.c
	$(CC) -o example example.c $(CFLAGS) $(LDFLAGS)

example_cnc: example_cnc.c
	$(CC) -o example_cnc example_cnc.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f example example_cnc
	rm -f test.db
