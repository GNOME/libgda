
# GConf schemas
schemadir   = $(sysconfdir)/gconf/schemas
schema_DATA = libgda.schemas

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

EXTRA_DIST = \
	$(schema_DATA)