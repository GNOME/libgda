/* GDA client library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#ifndef __GDA_TRANSACTION_STATUS_H__
#define __GDA_TRANSACTION_STATUS_H__

#include <glib-object.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_TRANSACTION_STATUS            (gda_transaction_status_get_type())
#define GDA_TRANSACTION_STATUS(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TRANSACTION_STATUS, GdaTransactionStatus))
#define GDA_TRANSACTION_STATUS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TRANSACTION_STATUS, GdaTransactionStatusClass))
#define GDA_IS_TRANSACTION_STATUS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_TRANSACTION_STATUS))
#define GDA_IS_TRANSACTION_STATUS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_TRANSACTION_STATUS))

typedef struct _GdaTransactionStatus        GdaTransactionStatus;
typedef struct _GdaTransactionStatusClass   GdaTransactionStatusClass;
typedef struct _GdaTransactionStatusEvent   GdaTransactionStatusEvent;

typedef enum {
	GDA_TRANSACTION_STATUS_EVENT_SAVEPOINT,
	GDA_TRANSACTION_STATUS_EVENT_SQL,
	GDA_TRANSACTION_STATUS_EVENT_SUB_TRANSACTION,
} GdaTransactionStatusEventType;

typedef enum {
	GDA_TRANSACTION_STATUS_STATE_OK,
	GDA_TRANSACTION_STATUS_STATE_FAILED
} GdaTransactionStatusState;

struct _GdaTransactionStatusEvent {
	GdaTransactionStatus         *trans;
	GdaTransactionStatusEventType type;
	union {
		gchar                *svp_name; /* save point name if this event corresponds to a new save point */
		gchar                *sql;      /* SQL to store SQL queries in transactions */
		GdaTransactionStatus *sub_trans;/* sub transaction event */
	} pl;
	GdaConnectionEvent           *conn_event;

	gpointer  reserved1;
	gpointer  reserved2;
};

struct _GdaTransactionStatus {
	GObject                    object;
	
	gchar                     *name;
	GdaTransactionIsolation    isolation_level;
	GdaTransactionStatusState  state;
	GList                     *events;
};

struct _GdaTransactionStatusClass {
	GObjectClass             parent_class;
	gpointer                 reserved[10];
};

GType                 gda_transaction_status_get_type (void);
GdaTransactionStatus *gda_transaction_status_new      (const gchar *name);


G_END_DECLS

#endif
