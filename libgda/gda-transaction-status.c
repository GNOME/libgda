/*
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
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

#include <libgda/gda-transaction-status.h>
#include <libgda/gda-transaction-status-private.h>
#include <string.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-connection-event.h>

#define PARENT_TYPE G_TYPE_OBJECT

static void gda_transaction_status_class_init (GdaTransactionStatusClass *klass);
static void gda_transaction_status_init       (GdaTransactionStatus *tstatus, GdaTransactionStatusClass *klass);
static void gda_transaction_status_finalize   (GObject *object);

static GObjectClass* parent_class = NULL;

/*
 * GdaTransactionStatus class implementation
 */

static void
gda_transaction_status_class_init (GdaTransactionStatusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_transaction_status_finalize;
}

static void
gda_transaction_status_init (GdaTransactionStatus *tstatus, G_GNUC_UNUSED GdaTransactionStatusClass *klass)
{
	tstatus->name = NULL;
	tstatus->isolation_level = GDA_TRANSACTION_ISOLATION_UNKNOWN;
	tstatus->events = NULL;
	tstatus->state = GDA_TRANSACTION_STATUS_STATE_OK;
}

static void
event_free (GdaTransactionStatusEvent *event)
{
	switch (event->type) {
	case GDA_TRANSACTION_STATUS_EVENT_SAVEPOINT:
		g_free (event->pl.svp_name);
		break;
	case GDA_TRANSACTION_STATUS_EVENT_SQL:
		g_free (event->pl.sql);
		break;
	case GDA_TRANSACTION_STATUS_EVENT_SUB_TRANSACTION:
		g_object_unref (event->pl.sub_trans);
		break;
	default:
		g_assert_not_reached ();
	}
	if (event->conn_event)
		g_object_unref (event->conn_event);
	
	g_free (event);
}

static void
gda_transaction_status_finalize (GObject *object)
{
	GdaTransactionStatus *tstatus = (GdaTransactionStatus *) object;

	g_return_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus));

	/* free memory */
	if (tstatus->name) {
		g_free (tstatus->name);
		tstatus->name = NULL;
	}

	if (tstatus->events) {
		g_list_foreach (tstatus->events, (GFunc) event_free, NULL);
		g_list_free (tstatus->events);
		tstatus->events = NULL;
	}

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_transaction_status_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static GTypeInfo info = {
			sizeof (GdaTransactionStatusClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_transaction_status_class_init,
			NULL, NULL,
			sizeof (GdaTransactionStatus),
			0,
			(GInstanceInitFunc) gda_transaction_status_init,
			0
		};
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (PARENT_TYPE, "GdaTransactionStatus", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}

/**
 * gda_transaction_status_new:
 * @name: name for the transaction
 *
 * Creates a new #GdaTransactionStatus object, which allows a fine-tune and
 * full control of transactions to be used with providers.
 *
 * Returns: (transfer full): the newly created object.
 */
GdaTransactionStatus *
gda_transaction_status_new (const gchar *name)
{
	GdaTransactionStatus *tstatus;

	tstatus = g_object_new (GDA_TYPE_TRANSACTION_STATUS, NULL);
	if (name)
		tstatus->name = g_strdup (name);

	return tstatus;
}

/**
 * gda_transaction_status_add_event_svp:
 *
 */
GdaTransactionStatusEvent *
gda_transaction_status_add_event_svp (GdaTransactionStatus *tstatus, const gchar *svp_name)
{
	GdaTransactionStatusEvent *ev;

	g_return_val_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus), NULL);
	g_return_val_if_fail (svp_name, NULL);

	ev = g_new0 (GdaTransactionStatusEvent, 1);
	ev->trans = tstatus;
	ev->type = GDA_TRANSACTION_STATUS_EVENT_SAVEPOINT;
	ev->pl.svp_name = g_strdup (svp_name);
	tstatus->events = g_list_append (tstatus->events, ev);

	return ev;
}

/**
 * gda_transaction_status_add_event_sql:
 *
 */
GdaTransactionStatusEvent *
gda_transaction_status_add_event_sql (GdaTransactionStatus *tstatus, const gchar *sql, GdaConnectionEvent *conn_event)
{
	GdaTransactionStatusEvent *ev;

	g_return_val_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus), NULL);
	g_return_val_if_fail (sql, NULL);

	ev = g_new0 (GdaTransactionStatusEvent, 1);
	ev->trans = tstatus;
	ev->type = GDA_TRANSACTION_STATUS_EVENT_SQL;
	ev->pl.sql = g_strdup (sql);
	if (conn_event) {
		ev->conn_event = conn_event;
		g_object_ref (conn_event);
	}
	tstatus->events = g_list_append (tstatus->events, ev);

	return ev;
}

/**
 * gda_transaction_status_add_event_sub:
 *
 */
GdaTransactionStatusEvent *
gda_transaction_status_add_event_sub (GdaTransactionStatus *tstatus, GdaTransactionStatus *sub_trans)
{
	GdaTransactionStatusEvent *ev;

	g_return_val_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus), NULL);
	g_return_val_if_fail (GDA_IS_TRANSACTION_STATUS (sub_trans), NULL);

	ev = g_new0 (GdaTransactionStatusEvent, 1);
	ev->trans = tstatus;
	ev->type = GDA_TRANSACTION_STATUS_EVENT_SUB_TRANSACTION;
	ev->pl.sub_trans = sub_trans;
	g_object_ref (ev->pl.sub_trans);
	tstatus->events = g_list_append (tstatus->events, ev);

	return ev;
}

void 
gda_transaction_status_free_events (GdaTransactionStatus *tstatus, GdaTransactionStatusEvent *event,
				    gboolean free_after)
{
	GList *node;

	g_return_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus));
	node = g_list_find (tstatus->events, event);
	g_return_if_fail (node);

	if (free_after) {
		GList *list = g_list_last (tstatus->events);
		GList *tmp;
		while (list != node) {
			event_free ((GdaTransactionStatusEvent *)(list->data));
			tmp = list->prev;
			tstatus->events = g_list_delete_link (tstatus->events, list);
			list = tmp;
		}
	}
	event_free (event);
	tstatus->events = g_list_delete_link (tstatus->events, node);
}

/**
 * gda_transaction_status_find:
 *
 * Returns: (transfer full) (allow-none):
 */
GdaTransactionStatus *
gda_transaction_status_find (GdaTransactionStatus *tstatus, const gchar *str, GdaTransactionStatusEvent **destev)
{
	GdaTransactionStatus *trans = NULL;
	GList *evlist;

	if (!tstatus)
		return NULL;

	g_return_val_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus), NULL);
	if (destev)
		*destev = NULL;

	if (!str)
		return gda_transaction_status_find_current (tstatus, destev, FALSE);

	if (tstatus->name && !strcmp (tstatus->name, str))
		return tstatus;

	for (evlist = tstatus->events; evlist && !trans; evlist = evlist->next) {
		GdaTransactionStatusEvent *ev = (GdaTransactionStatusEvent *)(evlist->data);
		switch (ev->type) {
		case GDA_TRANSACTION_STATUS_EVENT_SAVEPOINT:
			if (!strcmp (ev->pl.svp_name, str))
				trans = tstatus;
			break;
		case GDA_TRANSACTION_STATUS_EVENT_SQL:
			if (!strcmp (ev->pl.sql, str))
				trans = tstatus;
			break;
		case GDA_TRANSACTION_STATUS_EVENT_SUB_TRANSACTION:
			trans = gda_transaction_status_find (ev->pl.sub_trans, str, NULL);
			break;
		default:
			g_assert_not_reached ();
		}
		if (trans && destev)
			*destev = ev;
	}

	return trans;
}

/**
 * gda_transaction_status_find_current:
 *
 * Find a pointer to the "current" _unnamed_ transaction, which is the last
 * transaction if there are several nested transactions
 *
 * Returns: (transfer full) (allow-none):
 */
GdaTransactionStatus *
gda_transaction_status_find_current (GdaTransactionStatus *tstatus, GdaTransactionStatusEvent **destev, gboolean unnamed_only)
{
	GdaTransactionStatus *trans = NULL;
	GList *evlist;

	if (!tstatus)
		return NULL;
	g_return_val_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus), NULL);
	if (destev)
		*destev = NULL;

	for (evlist = tstatus->events; evlist && !trans; evlist = evlist->next) {
		GdaTransactionStatusEvent *ev = (GdaTransactionStatusEvent *)(evlist->data);
		if (ev->type == GDA_TRANSACTION_STATUS_EVENT_SUB_TRANSACTION)
			trans = gda_transaction_status_find_current (ev->pl.sub_trans, destev, unnamed_only);
		if (trans && destev && !(*destev))
			*destev = ev;
	}

	if (!trans && ((unnamed_only && !tstatus->name) || !unnamed_only))
			trans = tstatus;

	return trans;
}


#ifdef GDA_DEBUG
void
gda_transaction_status_dump (GdaTransactionStatus *tstatus, guint offset)
{
	gchar *str;
	GList *evlist;
	gchar *levels[] = {"GDA_TRANSACTION_ISOLATION_UNKNOWN",
			   "GDA_TRANSACTION_ISOLATION_READ_COMMITTED",
			   "GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED",
			   "GDA_TRANSACTION_ISOLATION_REPEATABLE_READ",
			   "GDA_TRANSACTION_ISOLATION_SERIALIZABLE"};
#define level_str(lev) ((lev < 4) ? levels[lev] : "UNKNOWN ISOLATION LEVEL")

	g_return_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus));

	str = g_new (gchar, offset+1);
	memset (str, ' ', offset);
	str [offset] = 0;

	g_print ("%sGdaTransactionStatus: %s (%s, %p)\n", str, tstatus->name ? tstatus->name : "(NONAME)", 
		 level_str (tstatus->isolation_level), tstatus);
	for (evlist = tstatus->events; evlist; evlist = evlist->next) {
		GdaTransactionStatusEvent *ev = (GdaTransactionStatusEvent *) (evlist->data);
		switch (ev->type) {
		case GDA_TRANSACTION_STATUS_EVENT_SAVEPOINT:
			g_print ("%s  *SAVE POINT %s (%p)\n", str, ev->pl.svp_name, ev);
			break;
		case GDA_TRANSACTION_STATUS_EVENT_SQL:
			g_print ("%s  *SQL %s (%p): %s\n", str, ev->pl.sql, ev, 
				 ev->conn_event ?  gda_connection_event_get_description (ev->conn_event) : "_no event_");
			break;
		case GDA_TRANSACTION_STATUS_EVENT_SUB_TRANSACTION:
			g_print ("%s  *SUB TRANSACTION (%p)\n", str, ev);
			gda_transaction_status_dump (ev->pl.sub_trans, offset + 5);
			break;
		default:
			g_assert_not_reached ();
		}
	}

	g_free (str);
}
#endif
