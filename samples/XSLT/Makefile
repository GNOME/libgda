CFLAGS = -Wall -g `pkg-config --cflags libgda-xslt-3.0`
LDFLAGS = `pkg-config --libs libgda-xslt-3.0`

all: transform

transform: transform.c
	$(CC) -o transform transform.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f transform 
	rm -f out.xml
