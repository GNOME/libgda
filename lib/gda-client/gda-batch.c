/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999-2001 Rodrigo Moya
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gda-batch.h"
#include <stdio.h>
#include <gobject/gsignal.h>

/* GdaBatch object signals */
enum {
	GDA_BATCH_BEGIN_TRANSACTION,
	GDA_BATCH_COMMIT_TRANSACTION,
	GDA_BATCH_ROLLBACK_TRANSACTION,
	GDA_BATCH_EXECUTE_COMMAND,
	GDA_BATCH_LAST_SIGNAL
};
static gint gda_batch_signals[GDA_BATCH_LAST_SIGNAL] = { 0, };

static void gda_batch_class_init (GdaBatchClass *klass);
static void gda_batch_init       (GdaBatch *job, GdaBatchClass *klass);
static void gda_batch_finalize   (GObject *object);

GType
gda_batch_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaBatchClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_batch_class_init,
			NULL,
			NULL,
			sizeof (GdaBatch),
			0,
			(GInstanceInitFunc) gda_batch_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaBatch", &info, 0);
	}
	return type;
}

static void
gda_batch_class_init (GdaBatchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	gda_batch_signals[GDA_BATCH_BEGIN_TRANSACTION] =
		g_signal_new ("begin_transaction",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaBatchClass, begin_transaction),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	gda_batch_signals[GDA_BATCH_COMMIT_TRANSACTION] =
		g_signal_new ("commit_transaction",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaBatchClass, commit_transaction),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	gda_batch_signals[GDA_BATCH_ROLLBACK_TRANSACTION] =
		g_signal_new ("rollback_transaction",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaBatchClass, rollback_transaction),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	gda_batch_signals[GDA_BATCH_EXECUTE_COMMAND] =
		g_signal_new ("execute_command",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaBatchClass, execute_command),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	object_class->finalize = gda_batch_finalize;
	klass->begin_transaction = 0;
	klass->commit_transaction = 0;
	klass->rollback_transaction = 0;
}

static void
gda_batch_init (GdaBatch *job, GdaBatchClass *klass)
{
	g_return_if_fail (GDA_IS_BATCH (job));

	job->cnc = NULL;
	job->transaction_mode = TRUE;
	job->is_running = FALSE;
	job->commands = NULL;
}

static void
gda_batch_finalize (GObject *object)
{
	GObjectClass *parent_class;
	GdaBatch *job = (GdaBatch *) object;

	g_return_if_fail (GDA_IS_BATCH (job));

	gda_batch_clear (job);

	parent_class = G_OBJECT_CLASS (g_type_class_peek (G_TYPE_OBJECT));
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

/**
 * gda_batch_new
 *
 * Creates a new #GdaBatch object, which can be used in applications
 * to simulate a transaction, that is, a series of commands which will
 * be committed only if only all of them succeed. If any of the commands
 * return an error when executed, all the changes are rolled back (by
 * calling #gda_connection_rollback_transaction).
 *
 * Although, this behavior is configurable. You can also use it as a way
 * of sending several commands to the underlying database, regardless
 * of the errors found in the process.
 *
 * Returns: a pointer to the new object, or NULL on error
 */
GdaBatch *
gda_batch_new (void)
{
	return GDA_BATCH (g_object_new (GDA_TYPE_BATCH, NULL));
}

/**
 * gda_batch_free
 * @job: a #GdaBatch object
 *
 * Destroy the given batch job object
 */
void
gda_batch_free (GdaBatch * job)
{
	g_return_if_fail (GDA_IS_BATCH (job));
	g_object_unref (G_OBJECT (job));
}

/**
 * gda_batch_load_file
 * @job: a #GdaBatch object
 * @filename: file name
 * @clean: clean up
 *
 * Load the given file as a set of commands into the given @job object.
 * The @clean parameter specifies whether to clean up the list of commands
 * before loading the given file or not.
 *
 * Returns: TRUE if successful, or FALSE on error
 */
gboolean
gda_batch_load_file (GdaBatch * job, const gchar * filename, gboolean clean)
{
	FILE *fp;

	g_return_val_if_fail (GDA_IS_BATCH (job), FALSE);
	g_return_val_if_fail (filename != 0, FALSE);

	/* clean up object if specified */
	if (clean)
		gda_batch_clear (job);

	/* open given file */
	if ((fp = fopen (filename, "r"))) {
		GString *buffer = g_string_new ("");
		gchar tmp[4097];
		gboolean rc = TRUE;

		while (fgets (tmp, sizeof (tmp) - 1, fp)) {
			g_string_append (buffer, tmp);
		}
		if (ferror (fp)) {
			g_warning ("error reading %s", filename);
			rc = FALSE;
		}
		else {
			gchar *cmd = strtok (buffer->str, ";");

			/* append commands */
			while (cmd) {
				gda_batch_add_command (job,
						       (const gchar *) cmd);
				cmd = strtok (NULL, ";");
			}
			rc = TRUE;
		}

		/* close file */
		g_string_free (buffer, TRUE);
		fclose (fp);
		return rc;
	}
	else
		g_warning ("error opening %s", filename);

	return FALSE;
}

/**
 * gda_batch_add_command
 * @job: a #GdaBatch object
 * @cmd: command string
 *
 * Adds a command to the list of commands to be executed
 */
void
gda_batch_add_command (GdaBatch * job, const gchar * cmd)
{
	gchar *str;

	g_return_if_fail (GDA_IS_BATCH (job));
	g_return_if_fail (cmd != 0);

	str = g_strdup (cmd);
	job->commands = g_list_append (job->commands, (gpointer) str);
}

/**
 * gda_batch_clear
 * job: a #GdaBatch object
 *
 * Clears the given #GdaBatch object. This means eliminating the
 * list of commands
 */
void
gda_batch_clear (GdaBatch * job)
{
	g_return_if_fail (GDA_IS_BATCH (job));

	if (GDA_IS_CONNECTION (job->cnc))
		gda_connection_free (job->cnc);
	job->cnc = NULL;
	job->is_running = FALSE;

	g_list_foreach (job->commands, g_free, 0);
	g_list_free (job->commands);
	job->commands = 0;
}

/**
 * gda_batch_start
 * @job: a #GdaBatch object
 *
 * Start the batch job execution. This function will return when
 * the series of commands is completed, or when an error is found
 *
 * Returns: TRUE if all goes well, or FALSE on error
 */
gboolean
gda_batch_start (GdaBatch * job)
{
	GList *node;

	g_return_val_if_fail (GDA_IS_BATCH (job), FALSE);
	g_return_val_if_fail (!job->is_running, FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (job->cnc), FALSE);
	g_return_val_if_fail (gda_connection_is_open (job->cnc), FALSE);

	node = g_list_first (job->commands);
	if (!node) {
		g_warning ("batch job without commands!");
		return FALSE;
	}

	/* start transaction if in transaction mode */
	if (job->transaction_mode &&
	    gda_connection_supports (
		    job->cnc,
		    GNOME_Database_Connection_FEATURE_TRANSACTIONS)) {
		if (gda_connection_begin_transaction (job->cnc) == -1) {
			/* FIXME: emit "error" signal */
			return FALSE;
		}
		g_signal_emit (G_OBJECT (job),
			       gda_batch_signals[GDA_BATCH_BEGIN_TRANSACTION], 0);
	}

	/* traverse list of commands */
	job->is_running = TRUE;
	while (node && job->is_running) {
		gchar *cmd = (gchar *) node->data;
		if (cmd && strlen (cmd) > 0) {
			GdaRecordset *recset;
			gulong reccount;

			/* execute command */
			gtk_signal_emit (G_OBJECT (job),
					 gda_batch_signals[GDA_BATCH_EXECUTE_COMMAND],
					 0, cmd);
			recset = gda_connection_execute (job->cnc, cmd,
							 &reccount, 0);
			if (recset) {
				gda_recordset_free (recset);
			}
			else {
				/* FIXME: emit "error" signal */
				if (job->transaction_mode &&
				    gda_connection_supports (
					    job->cnc,
					    GNOME_Database_Connection_FEATURE_TRANSACTIONS)) {
					/* rollback transaction */
					gda_connection_rollback_transaction (job->cnc);
					g_signal_emit (G_OBJECT (job),
						       gda_batch_signals[GDA_BATCH_ROLLBACK_TRANSACTION],
						       0);
					return FALSE;
				}
			}
		}
		node = g_list_next (node);
	}

	if (job->transaction_mode &&
	    gda_connection_supports (
		    job->cnc,
		    GNOME_Database_Connection_FEATURE_TRANSACTIONS)) {
		if (gda_connection_commit_transaction (job->cnc) == -1) {
			/* FIXME: emit "error" signal */
			return FALSE;
		}
		g_signal_emit (G_OBJECT (job),
			       gda_batch_signals[GDA_BATCH_COMMIT_TRANSACTION], 0);
	}

	job->is_running = FALSE;
	return TRUE;
}

/**
 * gda_batch_stop
 * @job: a #GdaBatch object
 *
 * Stop the execution of the given #GdaBatch object. This cancels the
 * the transaction (discarding all changes) if the #GdaBatch object
 * is in transaction mode
 */
void
gda_batch_stop (GdaBatch * job)
{
	g_return_if_fail (GDA_IS_BATCH (job));

	if (job->is_running) {
		job->is_running = FALSE;
	}
}

/**
 * gda_batch_is_running
 * @job: a #GdaBatch object
 */
gboolean
gda_batch_is_running (GdaBatch * job)
{
	g_return_val_if_fail (GDA_IS_BATCH (job), FALSE);
	return job->is_running;
}

/**
 * gda_batch_get_connection
 * @job: a #GdaBatch object
 *
 * Return the #GdaConnection object associated with the given
 * batch job
 */
GdaConnection *
gda_batch_get_connection (GdaBatch * job)
{
	g_return_val_if_fail (GDA_IS_BATCH (job), 0);
	return job->cnc;
}

/**
 * gda_batch_set_connection
 * @job: a #GdaBatch object
 * @cnc: a #GdaConnection object
 *
 * Associate a #GdaConnection object to the given batch job
 */
void
gda_batch_set_connection (GdaBatch * job, GdaConnection * cnc)
{
	g_return_if_fail (GDA_IS_BATCH (job));

	if (GDA_IS_CONNECTION (job->cnc))
		gda_connection_free (job->cnc);

	g_object_ref (G_OBJECT (cnc));
	job->cnc = cnc;
}

/**
 * gda_batch_get_transaction_mode
 * @job: a #GdaBatch object
 *
 * Returns the transaction mode for the given #GdaBatch object.
 * This mode specifies how the series of commands are treated. If
 * in transaction mode (TRUE), the execution is stopped whenever an
 * error is found, discarding all changes, whereas all data is
 * committed if no error is found.
 *
 * On the other hand, if transaction mode is disabled, the execution
 * continues until the end regardless of any error found.
 */
gboolean
gda_batch_get_transaction_mode (GdaBatch * job)
{
	g_return_val_if_fail (GDA_IS_BATCH (job), FALSE);
	return job->transaction_mode;
}

/**
 * gda_batch_set_transaction_mode
 * @job: a #GdaBatch object
 * @mode: transaction mode
 *
 * Enable/disable transaction mode for the given #GdaBatch
 * object. Transaction mode is enabled by default
 */
void
gda_batch_set_transaction_mode (GdaBatch * job, gboolean mode)
{
	g_return_if_fail (GDA_IS_BATCH (job));
	job->transaction_mode = mode;
}
