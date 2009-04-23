/* GDA oracle provider
 * Copyright (C) 1998 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *         Vivien Malerba <malerba@gnome-db.org>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
 *         Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __GDA_ORACLE_UTIL_H__
#define __GDA_ORACLE_UTIL_H__

#include "gda-oracle.h"

G_BEGIN_DECLS

#define gda_oracle_check_result(result, cnc, cdata, type, msg) \
        (((result) == OCI_SUCCESS || (result) == OCI_SUCCESS_WITH_INFO) \
	 ? NULL : _gda_oracle_handle_error ((result), (cnc), (cdata),	\
					    (type), (msg), __FILE__, __LINE__))

#define gda_oracle_blob_type(sqltype) \
        ((((sqltype) == SQLT_BFILEE) || ((sqltype) == SQLT_CFILEE)) ? OCI_DTYPE_FILE : OCI_DTYPE_LOB)


typedef struct {
	OCIDefine *hdef;
        OCIParam *pard;
        sb2 indicator;
        ub2 sql_type;
        ub2 defined_size;
        gpointer value;
        GType g_type;
} GdaOracleValue;

GdaConnectionEvent *_gda_oracle_make_error (dvoid *hndlp, ub4 type, const gchar *file, gint line);
GdaConnectionEvent *_gda_oracle_handle_error (gint result, GdaConnection *cnc,
					      OracleConnectionData *cdata,
					      ub4 type, const gchar *msg,
					      const gchar *file, gint line);
GType               _oracle_sqltype_to_g_type (const ub2 sqltype);
gchar *             _oracle_sqltype_to_string (const ub2 sqltype);
gchar *             _gda_oracle_value_to_sql_string (GValue *value);
GdaOracleValue     *_gda_value_to_oracle_value (const GValue *value);
void                _gda_oracle_set_value (GValue *value,
					   GdaOracleValue *ora_value,
					   GdaConnection *cnc);
void                _gda_oracle_value_free (GdaOracleValue *ora_value);



G_END_DECLS

#endif

