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

#if !defined(__gda_transaction_h__)
#  define __gda_transaction_h__

#include <glib-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_TRANSACTION            (gda_transaction_get_type())
#define GDA_TRANSACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TRANSACTION, GdaTransaction))
#define GDA_TRANSACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TRANSACTION, GdaTransactionClass))
#define GDA_IS_TRANSACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_TRANSACTION))
#define GDA_IS_TRANSACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_TRANSACTION))

typedef struct _GdaTransaction        GdaTransaction;
typedef struct _GdaTransactionClass   GdaTransactionClass;
typedef struct _GdaTransactionPrivate GdaTransactionPrivate;

struct _GdaTransaction {
	GObject object;
	GdaTransactionPrivate *priv;
};

struct _GdaTransactionClass {
	GObjectClass parent_class;
};

GType           gda_transaction_get_type (void);
GdaTransaction *gda_transaction_new (const gchar *name);

typedef enum {
	GDA_TRANSACTION_ISOLATION_UNKNOWN,
	GDA_TRANSACTION_ISOLATION_READ_COMMITTED,
	GDA_TRANSACTION_ISOLATION_READ_UNCOMMITTED,
	GDA_TRANSACTION_ISOLATION_REPEATABLE_READ
} GdaTransactionIsolation;

GdaTransactionIsolation gda_transaction_get_isolation_level (GdaTransaction *xaction);
void                    gda_transaction_set_isolation_level (GdaTransaction *xaction,
							     GdaTransactionIsolation level);

const gchar            *gda_transaction_get_name (GdaTransaction *xaction);
void                    gda_transaction_set_name (GdaTransaction *xaction, const gchar *name);

G_END_DECLS

#endif
