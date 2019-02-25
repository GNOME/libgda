/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_VCONNECTION_DATA_MODEL_H__
#define __GDA_VCONNECTION_DATA_MODEL_H__

#include "gda-virtual-connection.h"
#include <libgda/sql-parser/gda-statement-struct-parts.h>

#define GDA_TYPE_VCONNECTION_DATA_MODEL            (gda_vconnection_data_model_get_type())

G_BEGIN_DECLS

typedef struct _GdaVconnectionDataModelSpec  GdaVconnectionDataModelSpec;
typedef struct _GdaVconnectionDataModelFilter GdaVconnectionDataModelFilter;

/**
 * GdaVconnectionDataModelCreateColumnsFunc:
 * @Param1: a pointer to a #GdaVconnectionDataModelSpec structure
 * @Param2: a place to store errors, or %NULL
 * @Returns: (element-type GdaColumn) (transfer full): a new list of #GdaColumn objects
 *
 * Function called to create the virtual table's columns, as #GdaColumn objects.
 */
typedef GList *(*GdaVconnectionDataModelCreateColumnsFunc) (GdaVconnectionDataModelSpec *, GError **);

/**
 * GdaVconnectionDataModelCreateModelFunc:
 * @Param1: a pointer to a #GdaVconnectionDataModelSpec structure
 * @Returns: (transfer full): a new #GdaDataModel
 *
 * Function called to create a #GdaDataModel object, called when a virtual table's data need to
 * be accessed, and when optimization is not handled.
 */
typedef GdaDataModel *(*GdaVconnectionDataModelCreateModelFunc) (GdaVconnectionDataModelSpec *);

/**
 * GdaVconnectionDataModelFunc:
 * @Param1: (nullable): a pointer to a #GdaDataModel
 * @Param2: the name of the table represented by @Param1
 * @Param3: a data pointer, passed as last ergument to gda_vconnection_data_model_foreach()
 *
 * This function is called for every #GdaDataModel representing a table in a #GdaVconnectionDataModel
 * connection, when using the gda_vconnection_data_model_foreach() method.
 */
typedef void (*GdaVconnectionDataModelFunc) (GdaDataModel *, const gchar *, gpointer );

/**
 * GdaVconnectionDataModelFilter:
 *
 * This structure contains data which should be analysed to produce a data model (used as data 
 * for a virtual table) when a #GdaVconnectionDataModelCreateFModelFunc is called. The structure
 * contains an input part (which should not be modified) and and output part (it is
 * closely mapped with SQLite's sqlite3_index_info type).
 *
 * A pointer to this structure is passed to the #GdaVconnectionDataModelParseFilterFunc function
 * and the function has to modify the variables in the output part (marked as *Outputs*).
 *
 * The @idxNum and @idxPointer are passed to the #GdaVconnectionDataModelCreateFModelFunc function call
 * and they represent nothing specific except that the GdaVconnectionDataModelParseFilterFunc and
 * GdaVconnectionDataModelCreateFModelFunc functions need to agree on their meaning.
 *
 * See the gda-vconnection-hub.c file for an usage example.
 */
struct _GdaVconnectionDataModelFilter {
	/* Inputs */
	int nConstraint; /* Number of entries in aConstraint */
	struct GdaVirtualConstraint {
		int iColumn;       /* Column on left-hand side of constraint */
		GdaSqlOperatorType op;  /* Constraint operator, among _EQ, _LT, _LEQ, _GT, _GEQ, _REGEXP */
	} *aConstraint;            /* Table of WHERE clause constraints */
	int nOrderBy;              /* Number of terms in the ORDER BY clause */
	struct GdaVirtualOrderby {
		int iColumn;       /* Column number */
		gboolean desc;     /* TRUE for DESC.  FALSE for ASC. */
	} *aOrderBy;               /* The ORDER BY clause */

	/* Outputs */
	struct GdaVirtualConstraintUsage {
		int argvIndex;     /* if >0, constraint is part of argv to xFilter */
		gboolean omit;     /* Do not code a test for this constraint if TRUE */
	} *aConstraintUsage;
	int idxNum;                /* Number used to identify the index */
	gpointer idxPointer;       /* Pointer used to identify the index */
	gboolean orderByConsumed;  /* TRUE if output is already ordered */
	double estimatedCost;      /* Estimated cost of using this index */
};

/**
 * GdaVconnectionDataModelParseFilterFunc:
 * @Param1: a pointer to a #GdaVconnectionDataModelSpec structure
 * @Param2: a pointer to a #GdaVconnectionDataModelFilter structure
 *
 * This function actually analyses the proposed optimization and modified @Param2 to tell the database
 * engine how (if applicable) it implements the optimization.
 */
typedef void (*GdaVconnectionDataModelParseFilterFunc) (GdaVconnectionDataModelSpec *, GdaVconnectionDataModelFilter *); 

/**
 * GdaVconnectionDataModelCreateFModelFunc:
 * @Param1: a pointer to a #GdaVconnectionDataModelSpec structure
 * @Param2: the index number chosen to actually execute the optimization (from #GdaVconnectionDataModelFilter's #idxNum attribute)
 * @Param3: corresponds to the #GdaVconnectionDataModelFilter's #idxPointer attribute
 * @Param4: size of @Param5
 * @Param5: an array of #GValue, as specified by the #GdaVconnectionDataModelFilter's #aConstraintUsage's #argvIndex value
 * @Returns: (transfer full): a new #GdaDataModel
 *
 * Function called to create a #GdaDataModel object, called when a virtual table's data need to
 * be accessed, and when optimization is handled.
 */
typedef GdaDataModel *(*GdaVconnectionDataModelCreateFModelFunc) (GdaVconnectionDataModelSpec *, int, const char *, int, GValue **);

/**
 * GdaVconnectionDataModelSpec:
 * @data_model: (nullable): a #GdaDataModel, or %NULL
 * @create_columns_func: (nullable): a pointer to a #GdaVconnectionDataModelCreateColumnsFunc function, or %NULL
 * @create_model_func: (nullable): a pointer to a #GdaVconnectionDataModelCreateModelFunc function, or %NULL
 * @create_filter_func: (nullable): a pointer to a #GdaVconnectionDataModelParseFilterFunc function, or %NULL
 * @create_filtered_model_func: (nullable): a pointer to a #GdaVconnectionDataModelCreateFModelFunc function, or %NULL
 *
 * This structure holds all the information supplied to declare a virtual table using
 * gda_vconnection_data_model_add(). You don't need to provider pointers for all the functions and
 * for @data_model, but the following rules have to be respected:
 * <itemizedlist>
 *  <listitem><para>@data_model is not %NULL and all the function pointers are %NULL: this is the situation
 *   when the virtual table's contents is defined once by @data_model</para></listitem>
 *  <listitem><para>@data_model is %NULL and @create_columns_func is not %NULL:
 *     <itemizedlist>
 *        <listitem><para>@create_filtered_model_func is not %NULL: this is the situation where the
 *                        virtual table's associated data model handles filter optimizations.
 *                        @create_model_func is ignored in this case.
 *        </para></listitem>
 *        <listitem><para>@create_model_func is not %NULL: this is the situation where the
 *                        virtual table's associated data model does not handle filter optimizations
 *        </para></listitem>
 *     </itemizedlist>
 *  </para></listitem>
 * </itemizedlist>
 *
 * Note that if specifying a @create_filtered_model_func, you should also specifiy a @create_filter_func
 * function which is actually responsible for analysing the optimization.
 */
struct _GdaVconnectionDataModelSpec {
	GdaDataModel                             *data_model; //FIXME: Use GWeakRef
	GdaVconnectionDataModelCreateColumnsFunc  create_columns_func;
	GdaVconnectionDataModelCreateModelFunc    create_model_func;

	GdaVconnectionDataModelParseFilterFunc    create_filter_func;
	GdaVconnectionDataModelCreateFModelFunc   create_filtered_model_func;
};
#define GDA_VCONNECTION_DATA_MODEL_SPEC(x) ((GdaVconnectionDataModelSpec*)(x))


G_DECLARE_DERIVABLE_TYPE (GdaVconnectionDataModel, gda_vconnection_data_model, GDA, VCONNECTION_DATA_MODEL, GdaVirtualConnection)

struct _GdaVconnectionDataModelClass {
	GdaVirtualConnectionClass       parent_class;

	void                          (*vtable_created) (GdaVconnectionDataModel *cnc,
	                              const gchar *table_name);
	void                          (*vtable_dropped) (GdaVconnectionDataModel *cnc,
	                              const gchar *table_name);

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
};

/**
 * SECTION:gda-vconnection-data-model
 * @short_description: Virtual connection based on a list of GdaDataModel
 * @title: GdaVconnectionDataModel
 * @stability: Stable
 * @see_also: The #GdaVproviderDataModel provider to use to create such connection objects.
 *
 * The #GdaVconnectionDataModel is a virtual connection in which #GdaDataModel data models can be added
 * or removed, each representing a table in the connection, the gda_vconnection_data_model_add_model()
 * and gda_vconnection_data_model_remove() methods.
 *
 * Furthermore it is possible to declare tables without any associated data model yet,
 * the real data model being automatically created when the
 * table's data is needed. This allows more dynamic table's contents and filtering optimizations.
 * See the gda_vconnection_data_model_add() and gda_vconnection_data_model_remove() methods. The
 * filtering optimizations are based on SQLite's virtual table optimisations see
 * <ulink url="http://www.sqlite.org/cvstrac/wiki?p=VirtualTableBestIndexMethod">SQLite's documentation</ulink>
 * for some background information.
 */

gboolean            gda_vconnection_data_model_add       (GdaVconnectionDataModel *cnc, GdaVconnectionDataModelSpec *spec,
                                                          GDestroyNotify spec_free_func,
                                                          const gchar *table_name, GError **error);
gboolean            gda_vconnection_data_model_add_model (GdaVconnectionDataModel *cnc, 
                                                          GdaDataModel *model, const gchar *table_name, GError **error);
gboolean            gda_vconnection_data_model_remove    (GdaVconnectionDataModel *cnc, const gchar *table_name, GError **error);

GdaVconnectionDataModelSpec *gda_vconnection_data_model_get (GdaVconnectionDataModel *cnc, const gchar *table_name);
const gchar        *gda_vconnection_data_model_get_table_name (GdaVconnectionDataModel *cnc, GdaDataModel *model);
GdaDataModel       *gda_vconnection_data_model_get_model (GdaVconnectionDataModel *cnc, const gchar *table_name);

void                gda_vconnection_data_model_foreach   (GdaVconnectionDataModel *cnc, 
                                                          GdaVconnectionDataModelFunc func, gpointer data);

G_END_DECLS

#endif
