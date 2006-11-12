/* GDA client library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
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

#ifndef __GDA_TRANSACTION_STATUS_PRIVATE_H__
#define __GDA_TRANSACTION_STATUS_PRIVATE_H__

#include <glib-object.h>

G_BEGIN_DECLS

GdaTransactionStatusEvent *gda_transaction_status_add_event_svp (GdaTransactionStatus *tstatus, const gchar *svp_name);
GdaTransactionStatusEvent *gda_transaction_status_add_event_sql (GdaTransactionStatus *tstatus, const gchar *sql,
								 GdaConnectionEvent *conn_event);
GdaTransactionStatusEvent *gda_transaction_status_add_event_sub (GdaTransactionStatus *tstatus, GdaTransactionStatus *sub_trans);

void                       gda_transaction_status_free_events   (GdaTransactionStatus *tstatus, GdaTransactionStatusEvent *event,
								 gboolean free_after);

GdaTransactionStatus      *gda_transaction_status_find          (GdaTransactionStatus *tstatus, const gchar *str, 
								 GdaTransactionStatusEvent **destev);

GdaTransactionStatus      *gda_transaction_status_find_current  (GdaTransactionStatus *tstatus, GdaTransactionStatusEvent **destev,
								 gboolean unnamed_only);
#ifdef GDA_DEBUG
void                       gda_transaction_status_dump          (GdaTransactionStatus *tstatus, guint offset);
#endif

G_END_DECLS

#endif
