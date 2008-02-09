CFLAGS = -Wall -g `pkg-config --cflags libgda-4.0 json-glib-1.0`
LDFLAGS = `pkg-config --libs libgda-4.0 json-glib-1.0`

alldot = $(wildcard *.dot)
allpng = $(alldot:dot=png)
allsvg = $(alldot:dot=svg)

all: console

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

console: console.o graph.o
	$(CC) -o console console.o graph.o $(LDFLAGS)

png: $(allpng)
%.png: %.dot
	dot -Tpng $< > $@

svg: $(allsvg)
%.svg: %.dot
	dot -Tsvg $< > $@

clean:
	rm -f *~
	rm -f *.o
	rm -f console
	rm -f core*
	rm -f *.dot
	rm -f *.png
	rm -f *.svg
