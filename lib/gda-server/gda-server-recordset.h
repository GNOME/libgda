/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#if !defined(__gda_server_recordset_h__)
#  define __gda_server_recordset_h__

#include <bonobo/bonobo-xobject.h>
#include <GDA.h>
#include <gda-server-connection.h>
#include <gda-server-field.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define GDA_TYPE_SERVER_RECORDSET            (gda_server_recordset_get_type())
#define GDA_SERVER_RECORDSET(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_SERVER_RECORDSET, GdaServerRecordset)
#define GDA_SERVER_RECORDSET_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_SERVER_RECORDSET, GdaServerRecordsetClass)
#define GDA_IS_SERVER_RECORDSET(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_SERVER_RECORDSET)
#define GDA_IS_SERVER_RECORDSET_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_RECORDSET))

typedef struct _GdaServerRecordset      GdaServerRecordset;
typedef struct _GdaServerRecordsetClass GdaServerRecordsetClass;

struct _GdaServerRecordset {
	BonoboXObject        object;

	/* data */
	GdaServerConnection* cnc;
	GList*               fields;
	gulong               position;
	gboolean             at_begin;
	gboolean             at_end;

	gpointer             user_data;
};

struct _GdaServerRecordsetClass {
	BonoboXObjectClass parent_class;

	POA_GDA_Recordset__epv epv;
};

GtkType              gda_server_recordset_get_type (void);
GdaServerRecordset*  gda_server_recordset_new  (GdaServerConnection *cnc);
GdaServerConnection* gda_server_recordset_get_connection (GdaServerRecordset *recset);
void                 gda_server_recordset_add_field (GdaServerRecordset *recset,
                                                      GdaServerField *field);
GList*               gda_server_recordset_get_fields (GdaServerRecordset *recset);
gboolean             gda_server_recordset_is_at_begin (GdaServerRecordset *recset);
void                 gda_server_recordset_set_at_begin (GdaServerRecordset *recset,
                                                         gboolean at_begin);
gboolean             gda_server_recordset_is_at_end (GdaServerRecordset *recset);
void                 gda_server_recordset_set_at_end (GdaServerRecordset *recset,
                                                       gboolean at_end);
gpointer             gda_server_recordset_get_user_data (GdaServerRecordset *recset);
void                 gda_server_recordset_set_user_data (GdaServerRecordset *recset,
                                                          gpointer user_data);
void                 gda_server_recordset_free (GdaServerRecordset *recset);

gint                 gda_server_recordset_move_next (GdaServerRecordset *recset);
gint                 gda_server_recordset_move_prev (GdaServerRecordset *recset);
gint                 gda_server_recordset_close (GdaServerRecordset *recset);

#if defined(__cplusplus)
}
#endif

#endif
