CFLAGS = -Wall -g -DGDA_DISABLE_DEPRECATED `pkg-config --cflags libgda-4.0`
LDFLAGS = `pkg-config --libs libgda-4.0`

all: example background

example: example.c
	$(CC) -o example example.c $(CFLAGS) $(LDFLAGS)

background: background.c
	$(CC) -o background background.c $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f example background
