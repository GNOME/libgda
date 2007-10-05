CFLAGS = -Wall -g `pkg-config --cflags libgda-3.0`
LDFLAGS = `pkg-config --libs libgda-3.0`

all: ddl

ddl: ddl.c
	$(CC) -o ddl ddl.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f ddl
	rm -f ddl_db.db
