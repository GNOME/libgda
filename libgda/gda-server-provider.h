/*
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@ximian.com>
 * Copyright (C) 2004 - 2005 Alan Knowles <alank@src.gnome.org>
 * Copyright (C) 2005 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2013 Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_SERVER_PROVIDER_H__
#define __GDA_SERVER_PROVIDER_H__

#include <libgda/gda-server-operation.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-statement.h>
#include <libgda/gda-meta-store.h>
#include <libgda/gda-xa-transaction.h>
#include <libgda/thread-wrapper/gda-worker.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_PROVIDER            (gda_server_provider_get_type())

G_DECLARE_DERIVABLE_TYPE(GdaServerProvider, gda_server_provider, GDA, SERVER_PROVIDER, GObject)

struct _GdaServerProviderClass {
	GObjectClass parent_class;
	gpointer     functions_sets[8];
};


/* error reporting */
extern GQuark gda_server_provider_error_quark (void);
#define GDA_SERVER_PROVIDER_ERROR gda_server_provider_error_quark ()

typedef enum
{
        GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
	GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
	GDA_SERVER_PROVIDER_EMPTY_STMT_ERROR,
	GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
	GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR,
	GDA_SERVER_PROVIDER_OPERATION_ERROR,
	GDA_SERVER_PROVIDER_INTERNAL_ERROR,
	GDA_SERVER_PROVIDER_BUSY_ERROR,
	GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
	GDA_SERVER_PROVIDER_SERVER_VERSION_ERROR,
	GDA_SERVER_PROVIDER_DATA_ERROR,
	GDA_SERVER_PROVIDER_DEFAULT_VALUE_HANDLING_ERROR,
	GDA_SERVER_PROVIDER_MISUSE_ERROR,
	GDA_SERVER_PROVIDER_FILE_NOT_FOUND_ERROR
} GdaServerProviderError;

/**
 * SECTION:gda-server-provider
 * @short_description: Base class for all the DBMS providers
 * @title: GdaServerProvider
 * @stability: Stable
 * @see_also: #GdaConnection
 *
 * The #GdaServerProvider class is a virtual class which all the DBMS providers
 * must inherit, and implement its virtual methods.
 *
 * See the <link linkend="libgda-provider-class">Virtual methods for providers</link> section for more information
 * about how to implement the virtual methods.
 */

/* provider information */
const gchar           *gda_server_provider_get_name           (GdaServerProvider *provider);
const gchar           *gda_server_provider_get_version        (GdaServerProvider *provider);
const gchar           *gda_server_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);
gboolean               gda_server_provider_supports_feature   (GdaServerProvider *provider, GdaConnection *cnc,
							       GdaConnectionFeature feature);

/* types and values manipulation */
GdaDataHandler        *gda_server_provider_get_data_handler_g_type(GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GType for_type);
GdaDataHandler        *gda_server_provider_get_data_handler_dbms (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  const gchar *for_type);
GValue                *gda_server_provider_string_to_value       (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  const gchar *string, 
								  GType preferred_type,
								  gchar **dbms_type);
gchar                 *gda_server_provider_value_to_sql_string   (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GValue *from);
const gchar           *gda_server_provider_get_default_dbms_type (GdaServerProvider *provider,
								  GdaConnection *cnc,
								  GType type);
gchar                 *gda_server_provider_escape_string         (GdaServerProvider *provider,
								  GdaConnection *cnc, const gchar *str);
gchar                 *gda_server_provider_unescape_string       (GdaServerProvider *provider,
								  GdaConnection *cnc, const gchar *str);

/* actions with parameters */
gboolean               gda_server_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperationType type, GdaSet *options);
GdaServerOperation    *gda_server_provider_create_operation   (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperationType type, 
							       GdaSet *options, GError **error);
gchar                 *gda_server_provider_render_operation   (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperation *op, GError **error);
gboolean               gda_server_provider_perform_operation  (GdaServerProvider *provider, GdaConnection *cnc, 
							       GdaServerOperation *op, GError **error);

/* GdaStatement */
GdaSqlParser          *gda_server_provider_create_parser      (GdaServerProvider *provider, GdaConnection *cnc);

G_END_DECLS

#endif
