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
# GDA_COMMON 
#
GDA_COMMON = gda_common
GDA_COMMON_PY = $(GDA_COMMON).py
GDA_COMMON_CFLAGS = -I/opt/gnome/include/gda -I..
GDA_COMMON_LIBS = -lgda-common -lgnorba
GDA_COMMON_LIB = _$(GDA_COMMON)module.so

#
# GNOME
#
GNOME_CFLAGS = `gnome-config gnome gnomeui --cflags`
GNOME_LIBS = `gnome-config gnome gnomeui --libs`


GDA_COMMON_OBJS = gda_common.o



all: $(GDA_COMMON_LIB)

$(GDA_COMMON_LIB): $(GDA_COMMON_OBJS)
	gcc -shared $(GDA_COMMON_OBJS) $(GDA_COMMON_LIBS) $(GNOME_LIBS) -o $(GDA_COMMON_LIB)	
	chmod a+x $(GDA_COMMON_LIB)


install:
	cp $(GDA_COMMON_PY) $(GDA_COMMON_LIB) $(PYTHON_SITEPACKAGES) 

uninstall:
	rm -r $(PYTHON_SITEPACKAGES)/$(GDA_COMMON_PY) \
	      $(PYTHON_SITEPACKAGES)/$(GDA_COMMON_LIB)	

clean:
	rm -f *.o *.pyc *.pyo     \
	      $(GDA_COMMON_OBJS)    \
	      $(GDA_COMMON_LIB)

mr-proper: clean
	rm -f *~

.c.o:
	gcc $(CFLAGS) -I. $(PYTHON_INCLUDE) $(GNOME_CFLAGS) $(GDA_COMMON_CFLAGS) -c -o $@ $<

















