SHELL= /bin/sh 
SUBDIRS = BDB DDL DirDataModel F-Spot Report SimpleExample SqlParserConsole TableCopy Virtual XSLT MetaStore Tree
all:
	for dir in ${SUBDIRS} ; do ( cd $$dir ; ${MAKE} ) ; done
clean:
	for dir in ${SUBDIRS} ; do ( cd $$dir ; ${MAKE} clean ) ; done
	find . -name "core*" -exec rm '{}' ';'
