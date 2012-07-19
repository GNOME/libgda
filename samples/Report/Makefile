CFLAGS = -Wall -g `pkg-config --cflags libgda-report-5.0` 
LDFLAGS = `pkg-config --libs libgda-report-5.0`

HTML_STYLESHEET = /usr/share/xml/docbook/stylesheet/nwalsh/html/docbook.xsl
FO_STYLESHEET = /usr/share/xml/docbook/stylesheet/nwalsh/fo/docbook.xsl

all: customers-report customers-report-rml customers-report-docbook schema-report

customers-report: customers-report.c
	$(CC) -o customers-report customers-report.c $(CFLAGS) $(LDFLAGS)

customers-report-rml: customers-report-rml.c
	$(CC) -o customers-report-rml customers-report-rml.c $(CFLAGS) $(LDFLAGS)

customers-report-docbook: customers-report-docbook.c
	$(CC) -o customers-report-docbook customers-report-docbook.c $(CFLAGS) $(LDFLAGS)

schema-report: schema-report.c
	$(CC) -o schema-report schema-report.c $(CFLAGS) $(LDFLAGS)

html: customers-report.xml
	xsltproc --output customers-report.html --stringparam use.extensions 0 $(HTML_STYLESHEET) customers-report.xml

schema-html: schema-report.xml
	xsltproc --output schema-report.html --stringparam use.extensions 0 $(HTML_STYLESHEET) schema-report.xml

pdf: customers-report.xml
	fop -xml customers-report.xml -xsl $(FO_STYLESHEET) -pdf customers-report.pdf

clean:
	rm -f *~
	rm -f *.o
	rm -f customers-report customers-report-rml customers-report-docbook
	rm -f customers-report.xml customers-report.html customers-report.pdf
	rm -f customers-report-rml.pdf customers-report-rml.html
	rm -f customers-report-docbook.pdf customers-report-docbook.html
	rm -f schema-report schema-report.xml
