/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

G_DECLARE_DERIVABLE_TYPE(GdaXaTransaction, gda_xa_transaction, GDA, XA_TRANSACTION, GObject)
/* error reporting */
extern GQuark gda_xa_transaction_error_quark (void);
#define GDA_XA_TRANSACTION_ERROR gda_xa_transaction_error_quark ()

typedef enum
{
	GDA_XA_TRANSACTION_ALREADY_REGISTERED_ERROR,
	GDA_XA_TRANSACTION_DTP_NOT_SUPPORTED_ERROR,
	GDA_XA_TRANSACTION_CONNECTION_BRANCH_LENGTH_ERROR
} GdaXaTransactionError;

struct _GdaXaTransactionClass {
	GObjectClass             parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};


/**
 * GdaXaTransactionId:
 * @format: any number
 * @gtrid_length: number between 1 and 64
 * @bqual_length: number between 1 and 64
 * @data:
 */
struct _GdaXaTransactionId {
	guint32  format;
	gushort  gtrid_length;
	gushort  bqual_length;
	char     data [128];
};

typedef struct _GdaXaTransactionId      GdaXaTransactionId;

/**
 * SECTION:gda-xa-transaction
 * @short_description: Distributed transaction manager
 * @title: GdaXaTransaction
 * @stability: Stable
 * @see_also:
 *
 * The #GdaXaTransaction object acts as a distributed transaction manager: to make sure local transactions on several
 * connections (to possibly different databases and database types) either all succeed or all fail. For more information,
 * see the X/Open CAE document Distributed Transaction Processing: The XA Specification. 
 * This document is published by The Open Group and available at 
 * <ulink url="http://www.opengroup.org/public/pubs/catalog/c193.htm">http://www.opengroup.org/public/pubs/catalog/c193.htm</ulink>.
 *
 * The two phases commit protocol is implemented during the execution of a distributed transaction: modifications
 * made on any connection are first <emphasis>prepared</emphasis> (which means that they are store in the database), and
 * if that phase succeeded for all the involved connections, then the <emphasis>commit</emphasis> phase is executed
 * (where all the data previously stored during the <emphasis>prepare</emphasis> phase are actually committed).
 * That second phase may actually fail, but the distributed transaction will still be considered as successfull
 * as the data stored during the <emphasis>prepare</emphasis> phase can be committed afterwards.
 *
 * A distributed transaction involves the following steps:
 * <orderedlist>
 *   <listitem><para>Create a #GdaXaTransaction object</para></listitem>
 *   <listitem><para>Register the connections which will be part of the distributed transaction with that object
 *	using gda_xa_transaction_register_connection()</para></listitem>
 *   <listitem><para>Beging the distributed transaction using gda_xa_transaction_begin()</para></listitem>
 *   <listitem><para>Work individually on each connection as normally (make modifications)</para></listitem>
 *   <listitem><para>Commit the distributed transaction using gda_xa_transaction_commit()</para></listitem>
 *   <listitem><para>Discard the #GdaXaTransaction object using g_object_unref()</para></listitem>
 * </orderedlist>
 */

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
GType                     gda_xa_transaction_id_get_type             (void) G_GNUC_CONST;
gchar                    *gda_xa_transaction_id_to_string (const GdaXaTransactionId *xid);
GdaXaTransactionId       *gda_xa_transaction_string_to_id (const gchar *str);

G_END_DECLS

#endif
