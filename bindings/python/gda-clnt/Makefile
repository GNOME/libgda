# Gda-clnt python extension Makefile
#
# (c)2000 Alvaro Lopez Ortega


CFLAGS = -fpic -O2 -Wall

#
# Python
#
PYTHON_VERSION = 1.5
PYTHON_PREFIX = /usr/lib/python$(PYTHON_VERSION)
PYTHON_INCLUDE = -I/usr/include/python$(PYTHON_VERSION)
PYTHON_SITEPACKAGES = $(PYTHON_PREFIX)/site-packages

#
# GDA_CLNT 
#
GDA_CLNT = gda_clnt
GDA_CLNT_PY = $(GDA_CLNT).py
GDA_CLNT_CFLAGS = -I/opt/gnome/include/gda -I..
GDA_CLNT_LIBS = -lgda-client
GDA_CLNT_LIB = _$(GDA_CLNT)module.so

#
# GNOME
#
GNOME_CFLAGS = `gnome-config gnome gnomeui --cflags`
GNOME_LIBS = `gnome-config gnome gnomeui --libs`


GDA_CLNT_OBJS = gda_clnt.o



all: $(GDA_CLNT_LIB)

$(GDA_CLNT_LIB): $(GDA_CLNT_OBJS)
	gcc -shared $(GDA_CLNT_OBJS) $(GDA_CLNT_LIBS) $(GNOME_LIBS) -o $(GDA_CLNT_LIB)	
	chmod a+x $(GDA_CLNT_LIB)


install:
	cp $(GDA_CLNT_PY) $(GDA_CLNT_LIB) $(PYTHON_SITEPACKAGES) 

uninstall:
	rm -r $(PYTHON_SITEPACKAGES)/$(GDA_CLNT_PY) \
	      $(PYTHON_SITEPACKAGES)/$(GDA_CLNT_LIB)	

clean:
	rm -f *.o *.pyc *.pyo     \
	      $(GDA_CLNT_OBJS)    \
	      $(GDA_CLNT_LIB)

mr-proper: clean
	rm -f *~

.c.o:
	gcc $(CFLAGS) -I. $(PYTHON_INCLUDE) $(GNOME_CFLAGS) $(GDA_CLNT_CFLAGS) -c -o $@ $<

















