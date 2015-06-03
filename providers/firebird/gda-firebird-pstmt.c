/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-firebird-pstmt.h"

static void gda_firebird_pstmt_class_init (GdaFirebirdPStmtClass *klass);
static void gda_firebird_pstmt_init       (GdaFirebirdPStmt *pstmt, GdaFirebirdPStmtClass *klass);
static void gda_firebird_pstmt_finalize    (GObject *object);

static GObjectClass *parent_class = NULL;

/**
 * gda_firebird_pstmt_get_type
 *
 * Returns: the #GType of GdaFirebirdPStmt.
 */
GType
gda_firebird_pstmt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaFirebirdPStmtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_firebird_pstmt_class_init,
			NULL,
			NULL,
			sizeof (GdaFirebirdPStmt),
			0,
			(GInstanceInitFunc) gda_firebird_pstmt_init,
			0
		};

		g_mutex_lock (&registering);
		if (type == 0) {
#ifdef FIREBIRD_EMBED
			type = g_type_register_static (GDA_TYPE_PSTMT, "GdaFirebirdPStmtEmbed", &info, 0);
#else
			type = g_type_register_static (GDA_TYPE_PSTMT, "GdaFirebirdPStmt", &info, 0);
#endif
		}
		g_mutex_unlock (&registering);
	}
	return type;
}

static void 
gda_firebird_pstmt_class_init (GdaFirebirdPStmtClass *klass)
{
//WHERE_AM_I;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* virtual functions */
	object_class->finalize = gda_firebird_pstmt_finalize;
}

static void
gda_firebird_pstmt_init (GdaFirebirdPStmt *pstmt, GdaFirebirdPStmtClass *klass)
{
	g_return_if_fail (GDA_IS_PSTMT (pstmt));
//WHERE_AM_I;
	/* initialize specific parts of @pstmt */
	if (pstmt->stmt_h != 0){
		g_print("\t\tEXISTING PSTMT\n");
		if (!isc_dsql_free_statement(pstmt->status, &(pstmt->stmt_h), DSQL_close)){
			isc_print_status(pstmt->status);
		}
		
		pstmt->stmt_h = 0;
	}
	//else
		//g_print("\t\tNO STATEMENT\n");
	
	if (pstmt->sqlda != NULL){
		//g_print("\t\tEXISTING SQLDA\n");
		
		g_free(pstmt->sqlda);
		pstmt->sqlda = NULL;
	}
	
	if (pstmt->input_sqlda != NULL){
		//g_print("\t\tEXISTING SQLDA\n");
		g_free(pstmt->input_sqlda);
		pstmt->input_sqlda = NULL;
	}
	//else
		//g_print("\t\tNO SQLDA\n");
}

static void
gda_firebird_pstmt_finalize (GObject *object)
{
//WHERE_AM_I;

	GdaFirebirdPStmt *pstmt = (GdaFirebirdPStmt *) object;
	g_return_if_fail (GDA_IS_PSTMT (pstmt));

	/* free memory */
	//TO_IMPLEMENT; /* free some specific parts of @pstmt */
	if (pstmt->stmt_h != 0){
		g_print("\t\tCLOSE STATEMENT\n");
		if (!isc_dsql_free_statement(pstmt->status, &(pstmt->stmt_h), DSQL_close)){
			isc_print_status(pstmt->status);
		}
	}
	else{
		//g_print("\t\tNO STATEMENT TO CLOSE\n");
	}
	//g_free(pstmt->sqlda);
	//pstmt->sqlda = NULL;
	pstmt->stmt_h = 0;

	if (pstmt->sqlda != NULL){
		g_print("\t\tEXISTING SQLDA\n");
		g_free(pstmt->sqlda);
		pstmt->sqlda = NULL;
	}
	
	if (pstmt->input_sqlda != NULL){
		g_print("\t\tEXISTING SQLDA\n");
		g_free(pstmt->input_sqlda);
		pstmt->input_sqlda = NULL;
	}
	
	/* chain to parent class */
	parent_class->finalize (object);
}
