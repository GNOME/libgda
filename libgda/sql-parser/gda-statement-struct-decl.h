/* 
 * Copyright (C) 2007 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GDA_STATEMENT_STRUCT_DECL_H
#define _GDA_STATEMENT_STRUCT_DECL_H

#include <glib.h>
#include <glib-object.h>
#include <libgda/gda-dict.h>

/* error reporting */
extern GQuark gda_sql_error_quark (void);
#define GDA_SQL_ERROR gda_sql_error_quark ()

typedef enum {
	GDA_SQL_STRUCTURE_CONTENTS_ERROR,
	GDA_SQL_MALFORMED_IDENTIFIER_ERROR,
	GDA_SQL_DICT_ELEMENT_MISSING_ERROR
} GdaSqlErrorType;

/*
 * Struct declarations
 */
typedef struct _GdaSqlAnyPart   GdaSqlAnyPart;
typedef struct _GdaSqlStatement GdaSqlStatement;
typedef struct _GdaSqlStatementUnknown GdaSqlStatementUnknown;
typedef struct _GdaSqlStatementTransaction GdaSqlStatementTransaction;
typedef struct _GdaSqlStatementSelect GdaSqlStatementSelect;
typedef struct _GdaSqlStatementInsert GdaSqlStatementInsert;
typedef struct _GdaSqlStatementDelete GdaSqlStatementDelete;
typedef struct _GdaSqlStatementUpdate GdaSqlStatementUpdate;
typedef struct _GdaSqlStatementCompound GdaSqlStatementCompound;

/*
 * Statement type
 */
typedef enum {
	GDA_SQL_STATEMENT_SELECT,
	GDA_SQL_STATEMENT_INSERT,
	GDA_SQL_STATEMENT_UPDATE,
	GDA_SQL_STATEMENT_DELETE,
	GDA_SQL_STATEMENT_COMPOUND,

	GDA_SQL_STATEMENT_BEGIN,
	GDA_SQL_STATEMENT_ROLLBACK,
	GDA_SQL_STATEMENT_COMMIT,

	GDA_SQL_STATEMENT_SAVEPOINT,
	GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT,
	GDA_SQL_STATEMENT_DELETE_SAVEPOINT,

	GDA_SQL_STATEMENT_UNKNOWN,
	GDA_SQL_STATEMENT_NONE
} GdaSqlStatementType;


/*
 * Structures identification
 */
typedef enum {
	/* complete statements */
	GDA_SQL_ANY_STMT_SELECT = GDA_SQL_STATEMENT_SELECT,
	GDA_SQL_ANY_STMT_INSERT = GDA_SQL_STATEMENT_INSERT,
	GDA_SQL_ANY_STMT_UPDATE = GDA_SQL_STATEMENT_UPDATE,
	GDA_SQL_ANY_STMT_DELETE = GDA_SQL_STATEMENT_DELETE,
	GDA_SQL_ANY_STMT_COMPOUND = GDA_SQL_STATEMENT_COMPOUND,
	GDA_SQL_ANY_STMT_BEGIN = GDA_SQL_STATEMENT_BEGIN,
	GDA_SQL_ANY_STMT_ROLLBACK = GDA_SQL_STATEMENT_ROLLBACK,
	GDA_SQL_ANY_STMT_COMMIT = GDA_SQL_STATEMENT_COMMIT,
	GDA_SQL_ANY_STMT_SAVEPOINT = GDA_SQL_STATEMENT_SAVEPOINT,
	GDA_SQL_ANY_STMT_ROLLBACK_SAVEPOINT = GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT,
	GDA_SQL_ANY_STMT_DELETE_SAVEPOINT = GDA_SQL_STATEMENT_DELETE_SAVEPOINT,
	GDA_SQL_ANY_STMT_UNKNOWN = GDA_SQL_STATEMENT_UNKNOWN,
	
	/* individual parts */
	GDA_SQL_ANY_EXPR = 500,
	GDA_SQL_ANY_SQL_FIELD,
	GDA_SQL_ANY_SQL_TABLE,
	GDA_SQL_ANY_SQL_FUNCTION,
	GDA_SQL_ANY_SQL_OPERATION,
	GDA_SQL_ANY_SQL_CASE,
	GDA_SQL_ANY_SQL_SELECT_FIELD,
	GDA_SQL_ANY_SQL_SELECT_TARGET,
	GDA_SQL_ANY_SQL_SELECT_JOIN,
	GDA_SQL_ANY_SQL_SELECT_FROM,
	GDA_SQL_ANY_SQL_SELECT_ORDER
} GdaSqlAnyPartType;

struct _GdaSqlAnyPart {
	GdaSqlAnyPartType  type;
	GdaSqlAnyPart     *parent;
};

#define GDA_SQL_ANY_PART(x) ((GdaSqlAnyPart*)(x))
#define gda_sql_any_part_set_parent(a,p) \
	if (a) GDA_SQL_ANY_PART(a)->parent = GDA_SQL_ANY_PART(p)

/* 
 * Iteration
 */

/* returns FALSE if a recursive walking should be stopped (mandatory is @error is set) */
typedef gboolean (*GdaSqlForeachFunc) (GdaSqlAnyPart *, gpointer, GError **);

gboolean gda_sql_any_part_foreach (GdaSqlAnyPart *node, GdaSqlForeachFunc func, gpointer data, GError **error);

/*
 * Structure validation
 */
gboolean gda_sql_any_part_check_structure (GdaSqlAnyPart *node, GError **error);

/*
 * Contents' infos
 */
typedef struct {
	GdaSqlStatementType  type;
	gchar               *name;
	gpointer            (*construct) (void);
	void                (*free) (gpointer);
	gpointer            (*copy) (gpointer);
	gchar              *(*serialize) (gpointer);

	/* augmenting information precision using a dictionary */
	GdaSqlForeachFunc     check_structure_func;
} GdaSqlStatementContentsInfo;

#endif
