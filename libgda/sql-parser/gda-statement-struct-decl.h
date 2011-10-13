/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Murray Cumming <murrayc@murrayc.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef _GDA_STATEMENT_STRUCT_DECL_H
#define _GDA_STATEMENT_STRUCT_DECL_H

#include <glib.h>
#include <glib-object.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-meta-store.h>
#include <libgda/gda-meta-struct.h>

G_BEGIN_DECLS

/* error reporting */
extern GQuark gda_sql_error_quark (void);
#define GDA_SQL_ERROR gda_sql_error_quark ()
/*
 * GdaSqlError: (skip)
 *
 **/
typedef enum {
	GDA_SQL_STRUCTURE_CONTENTS_ERROR,
	GDA_SQL_MALFORMED_IDENTIFIER_ERROR,
	GDA_SQL_MISSING_IDENTIFIER_ERROR,
	GDA_SQL_VALIDATION_ERROR
} GdaSqlError;
/*
 * GdaSqlErrorType: (skip)
 *
 **/
typedef GdaSqlError GdaSqlErrorType;

/*
 * Struct declarations
 */
/*
 * GdaSqlAnyPart: (skip)
 *
 **/
typedef struct _GdaSqlAnyPart   GdaSqlAnyPart;
/*
 * GdaSqlStatement: (skip)
 *
 **/
typedef struct _GdaSqlStatement GdaSqlStatement;
/*
 * GdaSqlStatementUnknown: (skip)
 *
 **/
typedef struct _GdaSqlStatementUnknown GdaSqlStatementUnknown;
/*
 * GdaSqlStatementTransaction: (skip)
 *
 **/
typedef struct _GdaSqlStatementTransaction GdaSqlStatementTransaction;
/*
 * GdaSqlStatementSelect: (skip)
 *
 **/
typedef struct _GdaSqlStatementSelect GdaSqlStatementSelect;
/*
 * GdaSqlStatementInsert: (skip)
 *
 **/
typedef struct _GdaSqlStatementInsert GdaSqlStatementInsert;
/*
 * GdaSqlStatementDelete: (skip)
 *
 **/
typedef struct _GdaSqlStatementDelete GdaSqlStatementDelete;
/*
 * GdaSqlStatementUpdate: (skip)
 *
 **/
typedef struct _GdaSqlStatementUpdate GdaSqlStatementUpdate;
/*
 * GdaSqlStatementCompound: (skip)
 *
 **/
typedef struct _GdaSqlStatementCompound GdaSqlStatementCompound;

/*
 * Statement type
 */
/**
 * GdaSqlStatementType:
 * @GDA_SQL_STATEMENT_SELECT: a SELECT statement
 * @GDA_SQL_STATEMENT_INSERT: an INSERT statement
 * @GDA_SQL_STATEMENT_UPDATE: an UPDATE statement
 * @GDA_SQL_STATEMENT_DELETE: a DELETE statement
 * @GDA_SQL_STATEMENT_COMPOUND: a compound statement: multiple SELECT statements grouped together using an operator
 * @GDA_SQL_STATEMENT_BEGIN: start of transaction statement
 * @GDA_SQL_STATEMENT_ROLLBACK: transaction abort statement
 * @GDA_SQL_STATEMENT_COMMIT: transaction commit statement
 * @GDA_SQL_STATEMENT_SAVEPOINT: new savepoint definition statement
 * @GDA_SQL_STATEMENT_ROLLBACK_SAVEPOINT: return to savepoint statement
 * @GDA_SQL_STATEMENT_DELETE_SAVEPOINT: savepoint deletion statement
 * @GDA_SQL_STATEMENT_UNKNOWN: unknown statement, only identifies variables
 * @GDA_SQL_STATEMENT_NONE: not used
 *
 * Known types of statements
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
/**
 * GdaSqlAnyPartType: (skip)
 * @GDA_SQL_ANY_STMT_SELECT: structure is a #GdaSqlStatementSelect
 * @GDA_SQL_ANY_STMT_INSERT: structure is a #GdaSqlStatementInsert
 * @GDA_SQL_ANY_STMT_UPDATE: structure is a #GdaSqlStatementUpdate
 * @GDA_SQL_ANY_STMT_DELETE: structure is a #GdaSqlStatementDelete
 * @GDA_SQL_ANY_STMT_COMPOUND: structure is a #GdaSqlStatementCompound
 * @GDA_SQL_ANY_STMT_BEGIN: structure is a #GdaSqlStatementTransaction
 * @GDA_SQL_ANY_STMT_ROLLBACK: structure is a #GdaSqlStatementTransaction
 * @GDA_SQL_ANY_STMT_COMMIT: structure is a #GdaSqlStatementTransaction
 * @GDA_SQL_ANY_STMT_SAVEPOINT: structure is a #GdaSqlStatementTransaction
 * @GDA_SQL_ANY_STMT_ROLLBACK_SAVEPOINT: structure is a #GdaSqlStatementTransaction
 * @GDA_SQL_ANY_STMT_DELETE_SAVEPOINT: structure is a #GdaSqlStatementTransaction
 * @GDA_SQL_ANY_STMT_UNKNOWN: structure is a #GdaSqlStatementUnknown
 * @GDA_SQL_ANY_EXPR: structure is a #GdaSqlExpr
 * @GDA_SQL_ANY_SQL_FIELD: structure is a #GdaSqlField
 * @GDA_SQL_ANY_SQL_TABLE: structure is a #GdaSqlTable
 * @GDA_SQL_ANY_SQL_FUNCTION: structure is a #GdaSqlFunction
 * @GDA_SQL_ANY_SQL_OPERATION: structure is a #GdaSqlOperation
 * @GDA_SQL_ANY_SQL_CASE: structure is a #GdaSqlCase
 * @GDA_SQL_ANY_SQL_SELECT_FIELD: structure is a #GdaSqlSelectField
 * @GDA_SQL_ANY_SQL_SELECT_TARGET: structure is a #GdaSqlSelectTarget
 * @GDA_SQL_ANY_SQL_SELECT_JOIN: structure is a #GdaSqlSelectJoin
 * @GDA_SQL_ANY_SQL_SELECT_FROM: structure is a #GdaSqlSelectFrom
 * @GDA_SQL_ANY_SQL_SELECT_ORDER: structure is a #GdaSqlSelectOrder
 *
 * Type of part.
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


/**
 * GdaSqlAnyPart: (skip)
 * @type: type of structure, as a #GdaSqlAnyPartType enum.
 * @parent: pointer to the parent #GdaSqlAnyPart structure
 *
 * Base structure of which all structures (except #GdaSqlStatement) "inherit". It identifies, for each structure,
 * its type and its parent in the structure hierarchy.
 */
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
/**
 * GdaSqlForeachFunc: (skip)
 * @part: the current #GdaSqlAnyPart node
 * @data: user data passed to gda_sql_any_part_foreach().
 * @error: pointer to a place to store errors
 * @Returns: FALSE if the gda_sql_any_part_foreach() should stop at this point and fail
 *
 * Specifies the type of functions passed to gda_sql_any_part_foreach().
 */
typedef gboolean (*GdaSqlForeachFunc) (GdaSqlAnyPart *part, gpointer data, GError **error);

gboolean gda_sql_any_part_foreach (GdaSqlAnyPart *node, GdaSqlForeachFunc func, gpointer data, GError **error);

/*
 * Structure validation
 */
gboolean gda_sql_any_part_check_structure (GdaSqlAnyPart *node, GError **error);

/**
 * GdaSqlStatementContentsInfo: (skip)
 *
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
	GdaSqlForeachFunc     check_validity_func;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
	gpointer         _gda_reserved3;
	gpointer         _gda_reserved4;
} GdaSqlStatementContentsInfo;

/**
 * GdaSqlStatementCheckValidityData: (skip)
 *
 * Validation against a dictionary
 */
typedef struct {
	GdaConnection *cnc;
	GdaMetaStore  *store;
	GdaMetaStruct *mstruct;

	/*< private >*/
	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
	gpointer         _gda_reserved3;
	gpointer         _gda_reserved4;
} GdaSqlStatementCheckValidityData;

G_END_DECLS

#endif
