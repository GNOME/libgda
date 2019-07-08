/*
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 - 2014 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

/*
 * BLOB (Binary Large OBject) handling functions specific to each provider.
 */
#define G_LOG_DOMAIN "GDA-blob-op"

#include "gda-blob-op.h"
#include "gda-blob-op-impl.h"
#include "gda-lockable.h"
#include "gda-connection.h"
#include "gda-connection-internal.h"
#include "gda-server-provider-private.h"
#include "thread-wrapper/gda-worker.h"
#include "gda-value.h"

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(blob) (GDA_BLOB_OP_CLASS (G_OBJECT_GET_CLASS (blob)))
#define VFUNCTIONS(blob) (GDA_BLOB_OP_FUNCTIONS (CLASS(blob)->functions))
static void gda_blob_op_dispose   (GObject *object);


static void gda_blob_op_set_property (GObject *object,
				      guint param_id,
				      const GValue *value,
				      GParamSpec *pspec);
static void gda_blob_op_get_property (GObject *object,
				      guint param_id,
				      GValue *value,
				      GParamSpec *pspec);

typedef struct {
	GdaConnection *cnc;
	GdaWorker     *worker;
} GdaBlobOpPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaBlobOp, gda_blob_op, G_TYPE_OBJECT)

/* properties */
enum
{
        PROP_0,
        PROP_CNC,
};


static void
gda_blob_op_class_init (GdaBlobOpClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* properties */
        object_class->set_property = gda_blob_op_set_property;
        object_class->get_property = gda_blob_op_get_property;
        g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("connection", NULL,
                                                              "Connection used to fetch and write data",
                                                              GDA_TYPE_CONNECTION,
                                                              G_PARAM_WRITABLE | G_PARAM_READABLE | G_PARAM_CONSTRUCT_ONLY));

	/* virtual functions */
        object_class->dispose = gda_blob_op_dispose;
	klass->functions = g_new0 (GdaBlobOpFunctions, 1);
}

static void
gda_blob_op_init (GdaBlobOp *op)
{
}

static void
gda_blob_op_dispose (GObject *object)
{
	GdaBlobOp *op = (GdaBlobOp *) object;
	GdaBlobOpPrivate *priv = gda_blob_op_get_instance_private (op);

	if (priv->worker) {
		gda_worker_unref (priv->worker);
		priv->worker = NULL;
	}
	if (priv->cnc) {
		g_object_unref (priv->cnc);
		priv->cnc = NULL;
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_blob_op_parent_class)->dispose (object);
}

static void
gda_blob_op_set_property (GObject *object,
			  guint param_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
        GdaBlobOp *op = (GdaBlobOp *) object;
        GdaBlobOpPrivate *priv = gda_blob_op_get_instance_private (op);
        switch (param_id) {
        case PROP_CNC:
                priv->cnc = g_value_get_object (value);
                if (priv->cnc) {
                        g_object_ref (priv->cnc);
                        priv->worker = _gda_connection_get_worker (priv->cnc);
                        g_assert (priv->worker);
                        gda_worker_ref (priv->worker);
                }
		else
			g_warning ("GdaBlobOp created without any associated connection!");
                break;
	default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
	}
}

static void
gda_blob_op_get_property (GObject *object,
			  guint param_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	GdaBlobOp *op = (GdaBlobOp *) object;
        GdaBlobOpPrivate *priv = gda_blob_op_get_instance_private (op);
        switch (param_id) {
        case PROP_CNC:
		g_value_set_object (value, priv->cnc);
                break;
	default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
	}
}

typedef struct {
	GdaBlobOp *op;
	GdaBlob   *blob;
	glong      offset;
	glong      size;

	glong      retval;
} WorkerData;

static gpointer
worker_get_length (WorkerData *data, GError **error)
{
	if (VFUNCTIONS (data->op)->get_length != NULL)
		data->retval = VFUNCTIONS (data->op)->get_length (data->op);
	else
		data->retval = -1;
	return (gpointer) 0x01;
}

/**
 * gda_blob_op_get_length:
 * @op: an existing #GdaBlobOp
 * 
 * Returns: the length of the blob in bytes. In case of error, -1 is returned and the
 * provider should have added an error (a #GdaConnectionEvent) to the connection.
 */
glong
gda_blob_op_get_length (GdaBlobOp *op)
{
	g_return_val_if_fail (GDA_IS_BLOB_OP (op), -1);
	GdaBlobOpPrivate *priv = gda_blob_op_get_instance_private (op);
	if (priv) {
		if (! priv->cnc || !priv->worker) {
			g_warning ("Internal error: no connection of GdaWorker associated to blob operations object");
			return -1;
		}

		gda_lockable_lock ((GdaLockable*) priv->cnc); /* CNC LOCK */

		GMainContext *context;
		context = gda_server_provider_get_real_main_context (priv->cnc);

		WorkerData data;
		data.op = op;
		data.retval = -1;

		gpointer callval;
		gda_worker_do_job (priv->worker, context, 0, &callval, NULL,
				   (GdaWorkerFunc) worker_get_length, (gpointer) &data, NULL, NULL, NULL);
		if (context)
			g_main_context_unref (context);

		gda_lockable_unlock ((GdaLockable*) priv->cnc); /* CNC UNLOCK */

		if (callval == (gpointer) 0x01)
			return data.retval;
		else
			return -1;
	}
	else {
		if (VFUNCTIONS (op)->get_length != NULL)
			return VFUNCTIONS (op)->get_length (op);
		else
			return -1;
	}
}

static gpointer
worker_read (WorkerData *data, GError **error)
{
	if (VFUNCTIONS (data->op)->read != NULL)
		data->retval = VFUNCTIONS (data->op)->read (data->op, data->blob, data->offset, data->size);
	else
		data->retval = -1;
	return (gpointer) 0x01;
}

/**
 * gda_blob_op_read:
 * @op: a #GdaBlobOp
 * @blob: a #GdaBlob to read data to
 * @offset: offset to read from the start of the blob (starts at 0)
 * @size: maximum number of bytes to read.
 *
 * Reads a chunk of bytes from the BLOB accessible through @op into @blob.
 *
 * Returns: the number of bytes actually read. In case of error, -1 is returned and the
 * provider should have added an error to the connection.
 */
glong
gda_blob_op_read (GdaBlobOp *op, GdaBlob *blob, glong offset, glong size)
{
	g_return_val_if_fail (GDA_IS_BLOB_OP (op), -1);
	GdaBlobOpPrivate *priv = gda_blob_op_get_instance_private (op);

	if (priv) {
		if (! priv->cnc || !priv->worker) {
			g_warning ("Internal error: no connection of GdaWorker associated to blob operations object");
			return -1;
		}

		gda_lockable_lock ((GdaLockable*) priv->cnc); /* CNC LOCK */

		GMainContext *context;
		context = gda_server_provider_get_real_main_context (priv->cnc);

		WorkerData data;
		data.op = op;
		data.blob = blob;
		data.offset = offset;
		data.size = size;
		data.retval = -1;

		gpointer callval;
		gda_worker_do_job (priv->worker, context, 0, &callval, NULL,
				   (GdaWorkerFunc) worker_read, (gpointer) &data, NULL, NULL, NULL);
		if (context)
			g_main_context_unref (context);

		gda_lockable_unlock ((GdaLockable*) priv->cnc); /* CNC UNLOCK */

		if (callval == (gpointer) 0x01)
			return data.retval;
		else
			return -1;
	}
	else {
		if (VFUNCTIONS (op)->read != NULL)
			return VFUNCTIONS (op)->read (op, blob, offset, size);
		else
			return -1;
	}
}

/**
 * gda_blob_op_read_all:
 * @op: a #GdaBlobOp
 * @blob: a #GdaBlob to read data to
 *
 * Reads the whole contents of the blob manipulated by @op into @blob
 *
 * Returns: TRUE if @blob->data contains the whole BLOB manipulated by @op
 */
gboolean
gda_blob_op_read_all (GdaBlobOp *op, GdaBlob *blob)
{
	glong len;
	g_return_val_if_fail (GDA_IS_BLOB_OP (op), FALSE);
	g_return_val_if_fail (blob, FALSE);

	len = gda_blob_op_get_length (gda_blob_get_op (blob));
	if (len >= 0)
		return (gda_blob_op_read (gda_blob_get_op (blob), blob, 0, len) < 0) ? FALSE : TRUE;
	else
		return FALSE;
}

static gpointer
worker_write (WorkerData *data, GError **error)
{
	if (VFUNCTIONS (data->op)->write != NULL)
		data->retval = VFUNCTIONS (data->op)->write (data->op, data->blob, data->offset);
	else
		data->retval = -1;
	return (gpointer) 0x01;
}

/**
 * gda_blob_op_write:
 * @op: a #GdaBlobOp
 * @blob: a #GdaBlob which contains the data to write
 * @offset: offset to write from the start of the blob (starts at 0)
 *
 * Writes a chunk of bytes from a @blob to the BLOB accessible through @op, @blob is unchanged after
 * this call.
 *
 * If @blob has an associated #GdaBlobOp (ie. if @blob->op is not %NULL) then the data to be written
 * using @op is the data fetched using @blob->op.
 *
 * Returns: the number of bytes written. In case of error, -1 is returned and the
 * provider should have added an error to the connection.
 */
glong
gda_blob_op_write (GdaBlobOp *op, GdaBlob *blob, glong offset)
{
	g_return_val_if_fail (GDA_IS_BLOB_OP (op), -1);
	GdaBlobOpPrivate *priv = gda_blob_op_get_instance_private (op);

	if (priv) {
		if (! priv->cnc || !priv->worker) {
			g_warning ("Internal error: no connection of GdaWorker associated to blob operations object");
			return -1;
		}

		gda_lockable_lock ((GdaLockable*) priv->cnc); /* CNC LOCK */

		GMainContext *context;
		context = gda_server_provider_get_real_main_context (priv->cnc);

		WorkerData data;
		data.op = op;
		data.blob = blob;
		data.offset = offset;
		data.retval = -1;

		gpointer callval;
		gda_worker_do_job (priv->worker, context, 0, &callval, NULL,
				   (GdaWorkerFunc) worker_write, (gpointer) &data, NULL, NULL, NULL);
		if (context)
			g_main_context_unref (context);

		gda_lockable_unlock ((GdaLockable*) priv->cnc); /* CNC UNLOCK */

		if (callval == (gpointer) 0x01)
			return data.retval;
		else
			return -1;
	}
	else {
		if (VFUNCTIONS (op)->write != NULL)
			return VFUNCTIONS (op)->write (op, blob, offset);
		else
			return -1;
	}
}

static gpointer
worker_write_all (WorkerData *data, GError **error)
{
	if (VFUNCTIONS (data->op)->write_all != NULL)
		data->retval = VFUNCTIONS (data->op)->write_all (data->op, data->blob) ? 1 : 0;
	else
		data->retval = 0;
	return (gpointer) 0x01;
}

/**
 * gda_blob_op_write_all:
 * @op: a #GdaBlobOp
 * @blob: a #GdaBlob which contains the data to write
 *
 * Writes the whole contents of @blob into the blob manipulated by @op. If necessary the resulting
 * blob is truncated from its previous length.
 *
 * Returns: TRUE on success
 */
gboolean
gda_blob_op_write_all (GdaBlobOp *op, GdaBlob *blob)
{
	g_return_val_if_fail (GDA_IS_BLOB_OP (op), FALSE);
	GdaBlobOpPrivate *priv = gda_blob_op_get_instance_private (op);

	if (VFUNCTIONS (op)->write_all != NULL) {
		if (priv) {
			if (! priv->cnc || !priv->worker) {
				g_warning ("Internal error: no connection of GdaWorker associated to blob operations object");
				return -1;
			}

			gda_lockable_lock ((GdaLockable*) priv->cnc); /* CNC LOCK */

			GMainContext *context;
			context = gda_server_provider_get_real_main_context (priv->cnc);

			WorkerData data;
			data.op = op;
			data.blob = blob;
			data.retval = -1;

			gpointer callval;
			gda_worker_do_job (priv->worker, context, 0, &callval, NULL,
					   (GdaWorkerFunc) worker_write_all, (gpointer) &data, NULL, NULL, NULL);
			if (context)
				g_main_context_unref (context);

			gda_lockable_unlock ((GdaLockable*) priv->cnc); /* CNC UNLOCK */

			if (callval == (gpointer) 0x01)
				return data.retval ? TRUE : FALSE;
			else
				return FALSE;
		}
		else
			return VFUNCTIONS (op)->write_all (op, blob);
	}
	else {
		glong res;
		res = gda_blob_op_write (op, blob, 0);
		return res >= 0 ? TRUE : FALSE;
	}
}
