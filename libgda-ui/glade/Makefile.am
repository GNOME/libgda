glade_catalog_DATA = gdaui-catalog.xml

glade_pixmap_DATA = \
	widget-gdaui-gdauigrid.png \
	widget-gdaui-gdauilogin.png \
	widget-gdaui-gdauibasicform.png \
	widget-gdaui-gdauicombo.png \
	widget-gdaui-gdauirawgrid.png



EXTRA_DIST = \
	$(glade_catalog_DATA) \
	$(glade_pixmap_DATA)

if ENABLE_GLADE_SYSTEM_INSTALL
glade_catalogdir = $(GLADE_CATALOG)
glade_pixmapdir = $(GLADE_PIXMAP)
else
glade_catalogdir = $(datadir)/glade/catalogs
glade_pixmapdir = $(datadir)/glade/pixmaps
endif
