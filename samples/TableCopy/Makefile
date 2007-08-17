CFLAGS = -Wall -g `pkg-config --cflags libgda-3.0`
LDFLAGS = `pkg-config --libs libgda-3.0`

V_CFLAGS = -Wall -g `pkg-config --cflags libgda-virtual-3.0`
V_LDFLAGS = `pkg-config --libs libgda-virtual-3.0`

all: table-copy table-copy-easier

table-copy: table-copy.c common.o
	$(CC) -o table-copy common.o table-copy.c $(CFLAGS) $(LDFLAGS)

table-copy-easier: table-copy-easier.c common.o
	$(CC) -o table-copy-easier common.o table-copy-easier.c $(V_CFLAGS) $(V_LDFLAGS)

common.o: common.h common.c
	$(CC) -c -o common.o common.c $(CFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f table-copy table-copy-easier
	rm -f copy.db
