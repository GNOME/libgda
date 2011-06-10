/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

/*
 * Static types
 */
typedef enum {
	GDA_STYPE_INT,
	GDA_STYPE_STRING,
	GDA_STYPE_BOOLEAN,
	GDA_STYPE_DATE,
	GDA_STYPE_TIME,
	GDA_STYPE_TIMESTAMP,
	GDA_STYPE_INT64,
	GDA_STYPE_UINT64,
	GDA_STYPE_UINT,
	GDA_STYPE_FLOAT,
	GDA_STYPE_DOUBLE,
	GDA_STYPE_LONG,
	GDA_STYPE_ULONG,
	GDA_STYPE_NUMERIC,
	GDA_STYPE_BINARY,
	GDA_STYPE_BLOB,
	GDA_STYPE_CHAR,
	GDA_STYPE_SHORT,
	GDA_STYPE_GTYPE,
	GDA_STYPE_GEOMETRIC_POINT,

	GDA_STYPE_NULL /* MUST be last */
} GdaStaticType;
extern GType *static_types;
GdaStaticType gda_g_type_to_static_type (GType type);

/* 
   @indicator
   -2  item length > variable size so data truncated
   -1  value is NULL
   0   value is in host variable
   >0  item length > variable, indicator is length before truncation
*/
typedef struct {
	OCIDefine *hdef;
        OCIParam *pard;
	gboolean use_callback;

        sb2 indicator;
	ub2 rlen; /* length of data returned after fetch */
	ub2 rcode; /* column level return codes */
	ub4 xlen;

        ub2 sql_type; /* Oracle type */
        ub4 defined_size;
	sb2 precision;
	sb1 scale;

        gpointer value;
        GType g_type;
	GdaStaticType s_type;
} GdaOracleValue;

GdaConnectionEvent *_gda_oracle_make_error (GdaConnection *cnc, dvoid *hndlp, ub4 type, const gchar *file, gint line);
GdaConnectionEvent *_gda_oracle_handle_error (gint result, GdaConnection *cnc,
					      OracleConnectionData *cdata,
					      ub4 type, const gchar *msg,
					      const gchar *file, gint line);
GType               _oracle_sqltype_to_g_type (const ub2 sqltype, sb2 precision, sb1 scale);
ub2                 _g_type_to_oracle_sqltype (GType type);
gchar *             _oracle_sqltype_to_string (const ub2 sqltype);
GdaOracleValue     *_gda_value_to_oracle_value (const GValue *value);
void                _gda_oracle_set_value (GValue *value,
					   GdaOracleValue *ora_value,
					   GdaConnection *cnc);
void                _gda_oracle_value_free (GdaOracleValue *ora_value);

#ifdef GDA_DEBUG
void                _gda_oracle_test_keywords (void);
#endif
GdaSqlReservedKeywordsFunc _gda_oracle_get_reserved_keyword_func (OracleConnectionData *cdata);


G_END_DECLS

#endif

