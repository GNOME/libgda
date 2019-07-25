/*
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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
#define G_LOG_DOMAIN "GDA-transaction-status"

#include <libgda/gda-transaction-status.h>
#include <libgda/gda-transaction-status-private.h>
#include <string.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-connection-event.h>

#define PARENT_TYPE G_TYPE_OBJECT

typedef struct {
  gchar                     *name;
  GdaTransactionIsolation    isolation_level;
  GdaTransactionStatusState  state;
  GList                     *events;
} GdaTransactionStatusPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaTransactionStatus, gda_transaction_status, G_TYPE_OBJECT)

static void gda_transaction_status_finalize   (GObject *object);

/*
 * GdaTransactionStatus class implementation
 */

static void
gda_transaction_status_class_init (GdaTransactionStatusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_transaction_status_finalize;
}

static void
gda_transaction_status_init (GdaTransactionStatus *tstatus)
{
  GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (tstatus);
	priv->name = NULL;
	priv->isolation_level = GDA_TRANSACTION_ISOLATION_UNKNOWN;
	priv->events = NULL;
	priv->state = GDA_TRANSACTION_STATUS_STATE_OK;
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

	GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (tstatus);
	/* free memory */
	if (priv->name) {
		g_free (priv->name);
		priv->name = NULL;
	}

	if (priv->events) {
		g_list_free_full (priv->events, (GDestroyNotify) event_free);
		priv->events = NULL;
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_transaction_status_parent_class)->finalize (object);
}


GdaTransactionStatusEvent *
gda_transaction_status_event_copy (GdaTransactionStatusEvent *src) {
	GdaTransactionStatusEvent *cp = g_new0(GdaTransactionStatusEvent, 1);
	cp->trans = src->trans;
	cp->type = src->type;
	cp->pl.svp_name = g_strdup (src->pl.svp_name);
	cp->pl.sql = g_strdup (src->pl.sql);
	cp->pl.sub_trans = src->pl.sub_trans;
	cp->conn_event = src->conn_event;
  return cp;
}
void
gda_transaction_status_event_free (GdaTransactionStatusEvent *te) {
	g_free (te->pl.svp_name);
	g_free (te->pl.sql);
	g_free (te);
}

G_DEFINE_BOXED_TYPE(GdaTransactionStatusEvent, gda_transaction_status_event, gda_transaction_status_event_copy, gda_transaction_status_event_free)

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
  GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (tstatus);
	if (name)
		priv->name = g_strdup (name);

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

	GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (tstatus);

	ev = g_new0 (GdaTransactionStatusEvent, 1);
	ev->trans = tstatus;
	ev->type = GDA_TRANSACTION_STATUS_EVENT_SAVEPOINT;
	ev->pl.svp_name = g_strdup (svp_name);
	priv->events = g_list_append (priv->events, ev);

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
	GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (tstatus);

	ev = g_new0 (GdaTransactionStatusEvent, 1);
	ev->trans = tstatus;
	ev->type = GDA_TRANSACTION_STATUS_EVENT_SQL;
	ev->pl.sql = g_strdup (sql);
	if (conn_event) {
		ev->conn_event = conn_event;
		g_object_ref (conn_event);
	}
	priv->events = g_list_append (priv->events, ev);

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
	GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (tstatus);

	ev = g_new0 (GdaTransactionStatusEvent, 1);
	ev->trans = tstatus;
	ev->type = GDA_TRANSACTION_STATUS_EVENT_SUB_TRANSACTION;
	ev->pl.sub_trans = sub_trans;
	g_object_ref (ev->pl.sub_trans);
	priv->events = g_list_append (priv->events, ev);

	return ev;
}

void 
gda_transaction_status_free_events (GdaTransactionStatus *tstatus, GdaTransactionStatusEvent *event,
				    gboolean free_after)
{
	GList *node;

	g_return_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus));
	GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (tstatus);
	node = g_list_find (priv->events, event);
	g_return_if_fail (node);

	if (free_after) {
		GList *list = g_list_last (priv->events);
		GList *tmp;
		while (list != node) {
			event_free ((GdaTransactionStatusEvent *)(list->data));
			tmp = list->prev;
			priv->events = g_list_delete_link (priv->events, list);
			list = tmp;
		}
	}
	event_free (event);
	priv->events = g_list_delete_link (priv->events, node);
}

/**
 * gda_transaction_status_find:
 *
 * Returns: (transfer full) (nullable):
 */
GdaTransactionStatus *
gda_transaction_status_find (GdaTransactionStatus *tstatus, const gchar *str, GdaTransactionStatusEvent **destev)
{
	GdaTransactionStatus *trans = NULL;
	GList *evlist;

	if (!tstatus)
		return NULL;

	g_return_val_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus), NULL);
  GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (tstatus);
	if (destev)
		*destev = NULL;

	if (!str)
		return gda_transaction_status_find_current (tstatus, destev, FALSE);

	if (priv->name && !strcmp (priv->name, str))
		return tstatus;

	for (evlist = priv->events; evlist && !trans; evlist = evlist->next) {
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
 * Returns: (transfer full) (nullable):
 */
GdaTransactionStatus *
gda_transaction_status_find_current (GdaTransactionStatus *tstatus, GdaTransactionStatusEvent **destev, gboolean unnamed_only)
{
	GdaTransactionStatus *trans = NULL;
	GList *evlist;

	if (!tstatus)
		return NULL;
	g_return_val_if_fail (GDA_IS_TRANSACTION_STATUS (tstatus), NULL);
  GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (tstatus);
	if (destev)
		*destev = NULL;

	for (evlist = priv->events; evlist && !trans; evlist = evlist->next) {
		GdaTransactionStatusEvent *ev = (GdaTransactionStatusEvent *)(evlist->data);
		if (ev->type == GDA_TRANSACTION_STATUS_EVENT_SUB_TRANSACTION)
			trans = gda_transaction_status_find_current (ev->pl.sub_trans, destev, unnamed_only);
		if (trans && destev && !(*destev))
			*destev = ev;
	}

	if (!trans && ((unnamed_only && !priv->name) || !unnamed_only))
			trans = tstatus;

	return trans;
}

/**
 * gda_transaction_status_set_isolation_level:
 */
void
gda_transaction_status_set_isolation_level (GdaTransactionStatus *st, GdaTransactionIsolation il)
{
	g_return_if_fail (GDA_IS_TRANSACTION_STATUS (st));
	GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (st);
	priv->isolation_level = il;
}
/**
 * gda_transaction_status_get_isolation_level:
 */
GdaTransactionIsolation
gda_transaction_status_get_isolation_level (GdaTransactionStatus *st)
{
	g_return_val_if_fail (GDA_IS_TRANSACTION_STATUS (st), GDA_TRANSACTION_ISOLATION_UNKNOWN);
	GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (st);
	return priv->isolation_level;
}
/**
 * gda_transaction_status_set_state:
 */
void
gda_transaction_status_set_state (GdaTransactionStatus *st, GdaTransactionStatusState state)
{
	g_return_if_fail (GDA_IS_TRANSACTION_STATUS (st));
	GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (st);
	priv->state = state;
}

/**
 * gda_transaction_status_get_state:
 */
GdaTransactionStatusState
gda_transaction_status_get_state (GdaTransactionStatus *st)
{
	g_return_val_if_fail (GDA_IS_TRANSACTION_STATUS (st), GDA_TRANSACTION_STATUS_STATE_FAILED);
	GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (st);
	return priv->state;
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
	GdaTransactionStatusPrivate *priv = gda_transaction_status_get_instance_private (tstatus);

	str = g_new (gchar, offset+1);
	memset (str, ' ', offset);
	str [offset] = 0;

	g_print ("%sGdaTransactionStatus: %s (%s, %p)\n", str, priv->name ? priv->name : "(NONAME)",
		 level_str (priv->isolation_level), tstatus);
	for (evlist = priv->events; evlist; evlist = evlist->next) {
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
