if ODBC
GDA_ODBC_SERVER=gda-odbc-server
endif

if MYSQL
GDA_MYSQL_SERVER=gda-mysql-server
endif

if POSTGRES
GDA_POSTGRES_SERVER=gda-postgres-server
endif

if SYBASE
GDA_SYBASE_SERVER=gda-sybase-server
endif

if TDS
GDA_TDS_SERVER=gda-tds-server
endif

if ORACLE
GDA_ORACLE_SERVER=gda-oracle-server
endif

if INTERBASE
GDA_INTERBASE_SERVER=gda-interbase-server
endif

SUBDIRS = \
	$(GDA_ODBC_SERVER) \
	$(GDA_MYSQL_SERVER) \
	$(GDA_POSTGRES_SERVER) \
	$(GDA_SYBASE_SERVER) \
	$(GDA_TDS_SERVER) \
	$(GDA_ORACLE_SERVER) \
	$(GDA_INTERBASE_SERVER) \
	gda-default-server
