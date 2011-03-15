/*
 * Copyright (C) 2007 - 2011 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_VCONNECTION_DATA_MODEL_H__
#define __GDA_VCONNECTION_DATA_MODEL_H__

#include <virtual/gda-virtual-connection.h>
#include <sql-parser/gda-statement-struct-parts.h>

#define GDA_TYPE_VCONNECTION_DATA_MODEL            (gda_vconnection_data_model_get_type())
#define GDA_VCONNECTION_DATA_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_VCONNECTION_DATA_MODEL, GdaVconnectionDataModel))
#define GDA_VCONNECTION_DATA_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_VCONNECTION_DATA_MODEL, GdaVconnectionDataModelClass))
#define GDA_IS_VCONNECTION_DATA_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_VCONNECTION_DATA_MODEL))
#define GDA_IS_VCONNECTION_DATA_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_VCONNECTION_DATA_MODEL))

G_BEGIN_DECLS

typedef struct _GdaVconnectionDataModel      GdaVconnectionDataModel;
typedef struct _GdaVconnectionDataModelClass GdaVconnectionDataModelClass;
typedef struct _GdaVconnectionDataModelPrivate GdaVconnectionDataModelPrivate;
typedef struct _GdaVconnectionDataModelSpec  GdaVconnectionDataModelSpec;
typedef struct _GdaVconnectionDataModelFilter GdaVconnectionDataModelFilter;

/**
 * GdaVconnectionDataModelCreateColumnsFunc:
 * @Param1: 
 * @Param2: 
 * @Returns: 
 */
typedef GList        *(*GdaVconnectionDataModelCreateColumnsFunc) (GdaVconnectionDataModelSpec *, GError **);

/**
 * GdaVconnectionDataModelCreateModelFunc:
 * @Param1: 
 * @Returns: 
 */
typedef GdaDataModel *(*GdaVconnectionDataModelCreateModelFunc)   (GdaVconnectionDataModelSpec *);

/**
 * GdaVconnectionDataModelFunc:
 * @Param1: 
 * @Param2: 
 * @Param3: 
 */

typedef void (*GdaVconnectionDataModelFunc) (GdaDataModel *, const gchar *, gpointer );

/**
 * GdaVconnectionDataModelFilter:
 *
 * Enabling pre-filtering when creating a data model to be used as a table,
 * (structure closely mapped with SQLite's sqlite3_index_info type), to enable
 * the data model to perform some filter tasks itself.
 *
 * A pointer to this structure is passed to the GdaVconnectionDataModelParseFilterFunc function
 * and the function has to modify the variables in the *Outputs* section (and nowhere else)
 *
 * The @idxNum and @idxPointer are passed to the GdaVconnectionDataModelCreateFModelFunc function call
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
 * @Param1:
 * @Param2:
 */
typedef void          (*GdaVconnectionDataModelParseFilterFunc)   (GdaVconnectionDataModelSpec *, GdaVconnectionDataModelFilter *); 

/**
 * GdaVconnectionDataModelCreateFModelFunc:
 * @Param1: 
 * @Returns: 
 */
typedef GdaDataModel *(*GdaVconnectionDataModelCreateFModelFunc)  (GdaVconnectionDataModelSpec *,
								   int, const char *, int, GValue **);
/**
 * GdaVconnectionDataModelSpec:
 * @data_model: 
 * @create_columns_func: 
 * @create_model_func: 
 * @create_filter_func: 
 * @create_filtered_model_func:
 *
 */
struct _GdaVconnectionDataModelSpec {
	GdaDataModel                             *data_model;
	GdaVconnectionDataModelCreateColumnsFunc  create_columns_func;
	GdaVconnectionDataModelCreateModelFunc    create_model_func;

	GdaVconnectionDataModelParseFilterFunc    create_filter_func;
	GdaVconnectionDataModelCreateFModelFunc   create_filtered_model_func;
};
#define GDA_VCONNECTION_DATA_MODEL_SPEC(x) ((GdaVconnectionDataModelSpec*)(x))

struct _GdaVconnectionDataModel {
	GdaVirtualConnection            connection;
	GdaVconnectionDataModelPrivate *priv;
};

struct _GdaVconnectionDataModelClass {
	GdaVirtualConnectionClass       parent_class;

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
 * or removed, each representing a table in the connection.
 */

GType               gda_vconnection_data_model_get_type  (void) G_GNUC_CONST;

gboolean            gda_vconnection_data_model_add       (GdaVconnectionDataModel *cnc, GdaVconnectionDataModelSpec *spec, 
							  GDestroyNotify spec_free_func,
							  const gchar *table_name, GError **error);
gboolean            gda_vconnection_data_model_add_model (GdaVconnectionDataModel *cnc, 
							  GdaDataModel *model, const gchar *table_name, GError **error);
gboolean            gda_vconnection_data_model_remove    (GdaVconnectionDataModel *cnc, const gchar *table_name, GError **error);

const gchar        *gda_vconnection_data_model_get_table_name (GdaVconnectionDataModel *cnc, GdaDataModel *model);
GdaDataModel       *gda_vconnection_data_model_get_model (GdaVconnectionDataModel *cnc, const gchar *table_name);

void                gda_vconnection_data_model_foreach   (GdaVconnectionDataModel *cnc, 
							  GdaVconnectionDataModelFunc func, gpointer data);

G_END_DECLS

#endif
