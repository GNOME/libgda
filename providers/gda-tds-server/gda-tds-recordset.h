/* 
 * $Id$
 *
 * GNOME DB tds Provider
 * Copyright (C) 2000 Holger Thon
 * Copyright (C) 2000 Stephan Heinze
 * Copyright (C) 2000 Rodrigo Moya
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

#if !defined(__gda_tds_recordset_h__)
#  define __gda_tds_recordset_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <gda-server.h>
#include <glib.h>
#include <ctpublic.h>
#include "gda-tds.h"

/*
 * Per-object specific structures
 */

typedef struct _tds_Field {
  CS_INT     indicator;
  gchar      *data;
  CS_INT     datalen;
  CS_DATAFMT *fmt;
} tds_Field;

typedef struct _tds_Recordset {
  Gda_ServerConnection *cnc;
  Gda_ServerCommand    *cmd;
  tds_Connection *scnc;
  tds_Command    *scmd;
	
  CS_RETCODE   ret;
  CS_RETCODE   result_type;
  CS_INT       rows_cnt;
  gboolean     failed;
  CS_INT       colscnt;
  CS_DATAFMT   *datafmt;
  tds_Field *data;
} tds_Recordset;

gboolean gda_tds_recordset_new       (Gda_ServerRecordset *);
gint     gda_tds_recordset_move_next (Gda_ServerRecordset *);
gint     gda_tds_recordset_move_prev (Gda_ServerRecordset *);
gint     gda_tds_recordset_close     (Gda_ServerRecordset *);
void     gda_tds_recordset_free      (Gda_ServerRecordset *);

void     gda_tds_init_recset_fields (Gda_ServerError *,
                                        Gda_ServerRecordset *,
                                        tds_Recordset *,
                                        CS_RETCODE);

void gda_tds_field_fill_values(Gda_ServerRecordset *,
                                  tds_Recordset *);
gint gda_tds_row_result(gboolean forward,
                           Gda_ServerRecordset *,
                           tds_Recordset *,
                           CS_COMMAND *);

#endif
