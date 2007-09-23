CFLAGS = -Wall -g `pkg-config --cflags libgda-3.0`
LDFLAGS = `pkg-config --libs libgda-3.0`

all: find-duplicates

find-duplicates: find-duplicates.c
	$(CC) -o find-duplicates find-duplicates.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f find-duplicates
