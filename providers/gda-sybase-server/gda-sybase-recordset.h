/* 
 * $Id$
 *
 * GNOME DB sybase Provider
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2000 Stephan Heinze
 * Copyright (C) 2000 Holger Thon
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

#if !defined(__gda_sybase_recordset_h__)
#  define __gda_sybase_recordset_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <glib.h>
#include <ctpublic.h>
#include "gda-sybase.h"

/*
 * Per-object specific structures
 */

typedef struct _sybase_Field {
  CS_INT     indicator;
  gchar      *data;
  CS_INT     datalen;
  CS_DATAFMT *fmt;
} sybase_Field;

typedef struct _sybase_Recordset {
  GdaServerConnection *cnc;
  GdaServerCommand    *cmd;
  sybase_Connection *scnc;
  sybase_Command    *scmd;
	
  CS_RETCODE   ret;
  CS_RETCODE   result_type;
  CS_INT       rows_cnt;
  gboolean     failed;
  CS_INT       colscnt;
  CS_DATAFMT   *datafmt;
  sybase_Field *data;
} sybase_Recordset;

gboolean gda_sybase_recordset_new       (GdaServerRecordset *);
gint     gda_sybase_recordset_move_next (GdaServerRecordset *);
gint     gda_sybase_recordset_move_prev (GdaServerRecordset *);
gint     gda_sybase_recordset_close     (GdaServerRecordset *);
void     gda_sybase_recordset_free      (GdaServerRecordset *);

void     gda_sybase_init_recset_fields (GdaServerError *,
                                        GdaServerRecordset *,
                                        sybase_Recordset *,
                                        CS_RETCODE);

void gda_sybase_field_fill_values(GdaServerRecordset *,
                                  sybase_Recordset *);
gint gda_sybase_row_result(gboolean forward,
                           GdaServerRecordset *,
                           sybase_Recordset *,
                           CS_COMMAND *);

#endif
