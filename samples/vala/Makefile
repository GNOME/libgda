NULL = 
CFLAGS = \
	-Wall -g \
	`pkg-config --cflags libgdadata-5.0` \
	$(NULL)

LDFLAGS = `pkg-config --libs libgdadata-5.0` 

VALA_DATADIR=`pkg-config $VALA_MODULES --variable=datadir`
VAPIDIR="$VALA_DATADIR/vala/vapi"

VALAFLAGS = -g --save-temps --vapidir=$(VAPIDIR) --pkg libgda-5.0 --pkg gdadata-5.0 --pkg gee-0.8 --pkg libxml-2.0

all: sampledataobject sampledatamodeliterable

SampleDataObject.c: 
	valac $(VALAFLAGS) -C SampleDataObject.vala 

sampledataobject: SampleDataObject.c 
	$(CC) -o sampledataobject SampleDataObject.c $(CFLAGS) $(LDFLAGS)

SampleDataModelIterable.c:
	valac $(VALAFLAGS) -C SampleDataModelIterable.vala

sampledatamodeliterable: SampleDataModelIterable.c
	$(CC) -o sampledatamodeliterable SampleDataModelIterable.c $(CFLAGS) $(LDFLAGS)
    
clean:
	rm -f *~
	rm -f *.o
	rm -f *.c
	rm -f *.db
	rm sampledataobject sampledatamodeliterable
