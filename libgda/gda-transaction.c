/* GDA client library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <libgda/gda-transaction.h>

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaTransactionPrivate {
	gchar *name;
};

static void gda_transaction_class_init (GdaTransactionClass *klass);
static void gda_transaction_init       (GdaTransaction *xaction, GdaTransactionClass *klass);
static void gda_transaction_finalize   (GObject *object);

static GObjectClass* parent_class = NULL;

/*
 * GdaTransaction class implementation
 */

static void
gda_transaction_class_init (GdaTransactionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_transaction_finalize;
}

static void
gda_transaction_init (GdaTransaction *xaction, GdaTransactionClass *klass)
{
	/* allocate private structure */
	xaction->priv = g_new (GdaTransactionPrivate, 1);
	xaction->priv->name = NULL;
}

static void
gda_transaction_finalize (GObject *object)
{
	GdaTransaction *xaction = (GdaTransaction *) object;

	g_return_if_fail (GDA_IS_TRANSACTION (xaction));

	/* free memory */
	if (xaction->priv->name) {
		g_free (xaction->priv->name);
		xaction->priv->name = NULL;
	}

	g_free (xaction->priv);
	xaction->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_transaction_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaTransactionClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_transaction_class_init,
				NULL, NULL,
				sizeof (GdaTransaction),
				0,
				(GInstanceInitFunc) gda_transaction_init
			};
			type = g_type_register_static (PARENT_TYPE, "GdaTransaction", &info, 0);
		}
	}

	return type;
}

/**
 * gda_transaction_new
 * @name: name for the transaction.
 *
 * Create a new #GdaTransaction object, which allows a fine-tune and
 * full control of transactions to be used with providers.
 *
 * Returns: the newly created object.
 */
GdaTransaction *
gda_transaction_new (const gchar *name)
{
	GdaTransaction *xaction;

	xaction = g_object_new (GDA_TYPE_TRANSACTION, NULL);
	if (name)
		gda_transaction_set_name (xaction, name);

	return xaction;
}

/**
 * gda_transaction_get_name
 * @xaction: a #GdaTransaction object.
 *
 * Retrieve the name of the given transaction, as specified by the
 * client application (via a non-NULL parameter in the call to
 * #gda_transaction_new, or by calling #gda_transaction_set_name).
 * Note that some providers may set, when you call
 * #gda_connection_begin_transaction, the name of the transaction if
 * it's not been specified by the client application, so this function
 * may return, for some providers, values that you don't expect.
 *
 * Returns: the name of the transaction.
 */
const gchar *
gda_transaction_get_name (GdaTransaction *xaction)
{
	g_return_val_if_fail (GDA_IS_TRANSACTION (xaction), NULL);
	return (const gchar *) xaction->priv->name;
}

/**
 * gda_transaction_set_name
 * @xaction: a #GdaTransaction object.
 * @name: new name for the transaction.
 *
 * Set the name of the given transaction. This is very useful when
 * using providers that support named transactions.
 */
void
gda_transaction_set_name (GdaTransaction *xaction, const gchar *name)
{
	g_return_if_fail (GDA_IS_TRANSACTION (xaction));

	if (xaction->priv->name)
		g_free (xaction->priv->name);
	xaction->priv->name = g_strdup (name);
}
