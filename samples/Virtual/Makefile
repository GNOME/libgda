CFLAGS = -Wall -g `pkg-config --cflags libgda-virtual-3.0`
LDFLAGS = `pkg-config --libs libgda-virtual-3.0`

virtual-test: virtual-test.c
	$(CC) -o virtual-test virtual-test.c $(CFLAGS) $(LDFLAGS)
clean:
	rm -f *~
	rm -f *.o
	rm -f virtual-test
	rm -f results.csv
