SHELL= /bin/sh 

ifdef ENABLE_VALA_EXTENSIONS
    VALA_EXTENSIONS= vala
endif

SUBDIRS = AsyncExec BDB Blobs CustomUIPlugin DDL DirDataModel F-Spot Gir LdapBrowser MetaStore Report SimpleExample SimpleUIForm SqlBuilder SqlParserConsole TableCopy Tree Virtual WritableSelect XSLT $(VALA_EXTENSIONS)
all:
	for dir in ${SUBDIRS} ; do ( cd $$dir ; ${MAKE} ) ; done
clean:
	for dir in ${SUBDIRS} ; do ( cd $$dir ; ${MAKE} clean ) ; done
	find . -name "core*" -exec rm '{}' ';'
