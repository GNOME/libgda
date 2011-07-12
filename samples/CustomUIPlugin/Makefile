CFLAGS = -Wall -g -DGDA_DISABLE_DEPRECATED `pkg-config --cflags libgda-ui-5.0`
LDFLAGS = `pkg-config --libs libgda-ui-5.0`
PLUGINSDIR = `pkg-config --variable=pluginsdir libgda-ui-5.0`

all: custom-plugin.so

custom-plugin.so: libmain.o custom-entry-password.o

libmain.o: libmain.c
	$(CC) -fPIC -rdynamic $(CFLAGS) -c -o libmain.o libmain.c

custom-entry-password.o: custom-entry-password.c custom-entry-password.h
	$(CC) -fPIC -rdynamic $(CFLAGS) -c -o custom-entry-password.o custom-entry-password.c

custom-plugin.so: libmain.o custom-entry-password.o
	$(CC) -shared -Wl,-soname,custom-plugin.so -o custom-plugin.so libmain.o custom-entry-password.o

install: custom-plugin.so custom-entry-password.xml
	cp custom-plugin.so $(PLUGINSDIR)/
	cp custom-entry-password.xml $(PLUGINSDIR)/

uninstall:
	rm $(PLUGINSDIR)/custom-plugin.so
	rm $(PLUGINSDIR)/custom-entry-password.xml

clean:
	rm -f *~
	rm -f *.o
	rm -f *.so
