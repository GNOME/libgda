CFLAGS = -Wall -g `pkg-config --cflags libgda-5.0`
LDFLAGS = `pkg-config --libs libgda-5.0`
BDB_CFLAGS = -Wall -g
BDB_LDFLAGS = -ldb

all: fill access-raw access-custom

fill: fill.c common.h
	$(CC) -o fill fill.c $(BDB_CFLAGS) $(BDB_LDFLAGS)

access-raw: access-raw.c common.h
	$(CC) -o access-raw access-raw.c $(CFLAGS) $(LDFLAGS)

custom-bdb-model.o: custom-bdb-model.c custom-bdb-model.h
	$(CC) -c -o custom-bdb-model.o custom-bdb-model.c $(CFLAGS) $(GDA_CFLAGS)

access-custom: access-custom.c common.h custom-bdb-model.o
	$(CC) -o access-custom access-custom.c custom-bdb-model.o $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f fill access-raw access-custom
	rm -f access.db
