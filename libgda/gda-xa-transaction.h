/*
 * Copyright (C) 2008 - 2010 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_XA_TRANSACTION_H__
#define __GDA_XA_TRANSACTION_H__

#include <glib-object.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_XA_TRANSACTION            (gda_xa_transaction_get_type())
#define GDA_XA_TRANSACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XA_TRANSACTION, GdaXaTransaction))
#define GDA_XA_TRANSACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XA_TRANSACTION, GdaXaTransactionClass))
#define GDA_IS_XA_TRANSACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_XA_TRANSACTION))
#define GDA_IS_XA_TRANSACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_XA_TRANSACTION))

typedef struct _GdaXaTransaction        GdaXaTransaction;
typedef struct _GdaXaTransactionClass   GdaXaTransactionClass;
typedef struct _GdaXaTransactionPrivate GdaXaTransactionPrivate;
typedef struct _GdaXaTransactionId      GdaXaTransactionId;

/* error reporting */
extern GQuark gda_xa_transaction_error_quark (void);
#define GDA_XA_TRANSACTION_ERROR gda_xa_transaction_error_quark ()

typedef enum
{
        GDA_XA_TRANSACTION_ALREADY_REGISTERED_ERROR,
	GDA_XA_TRANSACTION_DTP_NOT_SUPPORTED_ERROR,
	GDA_XA_TRANSACTION_CONNECTION_BRANCH_LENGTH_ERROR
} GdaXaTransactionError;

struct _GdaXaTransaction {
	GObject                  object;
	GdaXaTransactionPrivate *priv;
};

struct _GdaXaTransactionClass {
	GObjectClass             parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

struct _GdaXaTransactionId {
	guint32  format;       /* any number */
	gushort  gtrid_length; /* 1-64 */
	gushort  bqual_length; /* 1-64 */
	char     data [128];
};

GType                     gda_xa_transaction_get_type             (void) G_GNUC_CONST;
GdaXaTransaction         *gda_xa_transaction_new                  (guint32 format, const gchar *global_transaction_id);

gboolean                  gda_xa_transaction_register_connection  (GdaXaTransaction *xa_trans, GdaConnection *cnc, 
								   const gchar *branch, GError **error);
void                      gda_xa_transaction_unregister_connection (GdaXaTransaction *xa_trans, GdaConnection *cnc);

gboolean                  gda_xa_transaction_begin  (GdaXaTransaction *xa_trans, GError **error);
gboolean                  gda_xa_transaction_commit (GdaXaTransaction *xa_trans, GSList **cnc_to_recover, GError **error);
gboolean                  gda_xa_transaction_rollback (GdaXaTransaction *xa_trans, GError **error);

gboolean                  gda_xa_transaction_commit_recovered (GdaXaTransaction *xa_trans, GSList **cnc_to_recover, 
							       GError **error);

/* utility functions */
gchar                    *gda_xa_transaction_id_to_string (const GdaXaTransactionId *xid);
GdaXaTransactionId       *gda_xa_transaction_string_to_id (const gchar *str);

G_END_DECLS

#endif
