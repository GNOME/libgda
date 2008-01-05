/* gda-server-provider-extra.h
 *
 * Copyright (C) 2005 - 2007 Vivien Malerba
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

#ifndef __GDA_SERVER_PROVIDER_EXTRA__
#define __GDA_SERVER_PROVIDER_EXTRA__

#include <libgda/gda-decl.h>
#include <libgda/gda-value.h>
#include <libgda/gda-connection.h>

G_BEGIN_DECLS

/*
 * GdaSqlParser associated to each provider
 */
GdaSqlParser *gda_server_provider_internal_get_parser (GdaServerProvider *prov);

/*
 * Help to implement providers, so the schemas return the same
 * number of columns and column titles across the providers.
 */
gint      gda_server_provider_get_schema_nb_columns (GdaConnectionSchema schema);
gboolean  gda_server_provider_init_schema_model     (GdaDataModel *model, GdaConnectionSchema schema);
gboolean  gda_server_provider_test_schema_model     (GdaDataModel *model, GdaConnectionSchema schema, GError **error);


/*
 * Help to implement the GdaDataHandler retreiving for the providers
 */
typedef struct {
	GdaConnection *cnc;
	GType          g_type;
	gchar         *dbms_type;
} GdaServerProviderHandlerInfo;

guint           gda_server_provider_handler_info_hash_func  (GdaServerProviderHandlerInfo *key);
gboolean        gda_server_provider_handler_info_equal_func (GdaServerProviderHandlerInfo *a, 
							     GdaServerProviderHandlerInfo *b);
void            gda_server_provider_handler_info_free       (GdaServerProviderHandlerInfo *info);

GdaDataHandler *gda_server_provider_handler_find            (GdaServerProvider *prov, GdaConnection *cnc, 
							     GType g_type, const gchar *dbms_type);
void            gda_server_provider_handler_declare         (GdaServerProvider *prov, GdaDataHandler *dh,
							     GdaConnection *cnc, 
							     GType g_type, const gchar *dbms_type);

/*
 * misc
 */
gboolean       gda_server_provider_blob_list_for_update     (GdaConnection *cnc, GdaQuery *query, 
							     GdaQuery **out_select, GError **error);
gboolean       gda_server_provider_blob_list_for_delete     (GdaConnection *cnc, GdaQuery *query, 
							     GdaQuery **out_select, GError **error);
gboolean       gda_server_provider_split_update_query       (GdaConnection *cnc, GdaQuery *query, 
							     GdaQuery **out_query, GError **error);
gboolean       gda_server_provider_select_query_has_blobs   (GdaConnection *cnc, GdaQuery *query, GError **error);

gchar         *gda_server_provider_find_file                (GdaServerProvider *prov, const gchar *inst_dir, const gchar *filename);
gchar         *gda_server_provider_load_file_contents       (const gchar *inst_dir, const gchar *data_dir, const gchar *filename);

G_END_DECLS

#endif



