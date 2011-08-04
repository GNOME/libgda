CFLAGS = -Wall -g -DGDA_DISABLE_DEPRECATED `pkg-config --cflags libgda-5.0`
LDFLAGS = `pkg-config --libs libgda-5.0`

all: ddl

ddl: ddl.c
	$(CC) -o ddl ddl.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f ddl
	rm -f ddl_db.db
