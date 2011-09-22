/*
 * Copyright (C) 2006 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
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

/**
 * GdaTransactionStatusEventType:
 * @GDA_TRANSACTION_STATUS_EVENT_SAVEPOINT: 
 * @GDA_TRANSACTION_STATUS_EVENT_SQL: 
 * @GDA_TRANSACTION_STATUS_EVENT_SUB_TRANSACTION:
 */
typedef enum {
	GDA_TRANSACTION_STATUS_EVENT_SAVEPOINT,
	GDA_TRANSACTION_STATUS_EVENT_SQL,
	GDA_TRANSACTION_STATUS_EVENT_SUB_TRANSACTION
} GdaTransactionStatusEventType;

/**
 * GdaTransactionStatusState:
 * @GDA_TRANSACTION_STATUS_STATE_OK:
 * @GDA_TRANSACTION_STATUS_STATE_FAILED:
 */
typedef enum {
	GDA_TRANSACTION_STATUS_STATE_OK,
	GDA_TRANSACTION_STATUS_STATE_FAILED
} GdaTransactionStatusState;

/**
 * GdaTransactionStatusEvent:
 * @trans: 
 * @type: 
 * @conn_event:
 */
struct _GdaTransactionStatusEvent {
	GdaTransactionStatus         *trans;
	GdaTransactionStatusEventType type;
	union {
		gchar                *svp_name; /* save point name if this event corresponds to a new save point */
		gchar                *sql;      /* SQL to store SQL queries in transactions */
		GdaTransactionStatus *sub_trans;/* sub transaction event */
	} pl;
	GdaConnectionEvent           *conn_event;

	/*< private >*/
	gpointer  _gda_reserved1;
	gpointer  _gda_reserved2;
};

/**
 * GdaTransactionStatus:
 * @name:
 * @isolation_level:
 * @state:
 * @events: (element-type Gda.TransactionStatusEvent):
 *
 */
struct _GdaTransactionStatus {
	GObject                    object;
	
	gchar                     *name;
	GdaTransactionIsolation    isolation_level;
	GdaTransactionStatusState  state;
	GList                     *events;

	/*< private >*/
	gpointer  _gda_reserved1;
	gpointer  _gda_reserved2;
};

struct _GdaTransactionStatusClass {
	GObjectClass             parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-transaction-status
 * @short_description: Keeps track of the transaction status of a connection
 * @title: GdaTransactionStatus
 * @stability: Stable
 * @see_also: #GdaConnection
 *
 * On any connection (as a #GdaConnection object), if the database provider used by the connection
 * supports it, transactions may be started, committed or rolledback, or savepoints added, removed or rolledback.
 * These operations can be performed using Libgda's API (such as gda_connection_begin_transaction()), or directly
 * using some SQL on the connection (usually a "BEGIN;" command). The #GdaTransactionStatus's aim is to 
 * make it easy to keep track of all the commands which have been issued on a connection regarding transactions.
 *
 * One #GdaTransactionStatus object is automatically attached to a #GdaConnection when a transaction is started, and
 * is destroyed when the transaction is finished. A pointer to this object can be fetched using
 * gda_connection_get_transaction_status() (beware that it should then not be modified). The end user is not
 * supposed to instantiate #GdaTransactionStatus objects
 *
 * #GdaTransactionStatus's attributes are directly accessible using the public members of the object.
 */

GType                 gda_transaction_status_get_type (void) G_GNUC_CONST;
GdaTransactionStatus *gda_transaction_status_new      (const gchar *name);


G_END_DECLS

#endif
