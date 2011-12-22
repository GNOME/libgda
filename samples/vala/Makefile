NULL = 
CFLAGS = \
	-Wall -g \
	`pkg-config --cflags libgdadata-5.0` \
	$(NULL)

LDFLAGS = `pkg-config --libs libgdadata-5.0` 

VALAFLAGS = --pkg libgda-5.0 --pkg gdadata-5.0 --pkg gee-0.8 --pkg libxml-2.0

all: sampledataobject 

SampleDataObject.c: 
	valac $(VALAFLAGS) -C SampleDataObject.vala 

sampledataobject: SampleDataObject.c 
	$(CC) -o sampledataobject SampleDataObject.c $(CFLAGS) $(LDFLAGS)


clean:
	rm -f *~
	rm -f *.o
	rm -f *.c
	rm -f *.db
	rm sampledataobject
