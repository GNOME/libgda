/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999,2000 Rodrigo Moya
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

#ifndef HAVE_GOBJECT
#  include <gtk/gtksignal.h>
#endif

/* GdaBatch object signals */
enum
{
	GDA_BATCH_BEGIN_TRANSACTION,
	GDA_BATCH_COMMIT_TRANSACTION,
	GDA_BATCH_ROLLBACK_TRANSACTION,
	GDA_BATCH_EXECUTE_COMMAND,
	GDA_BATCH_LAST_SIGNAL
};
static gint gda_batch_signals[GDA_BATCH_LAST_SIGNAL] = { 0, };

#ifdef HAVE_GOBJECT
static void gda_batch_class_init (GdaBatchClass * klass, gpointer data);
static void gda_batch_init (GdaBatch * job, GdaBatchClass * klass);
#else
static void gda_batch_class_init (GdaBatchClass * klass);
static void gda_batch_init (GdaBatch * job);
#endif

#ifdef HAVE_GOBJECT
GType
gda_batch_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GdaBatchClass),	/* class_size */
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) gda_batch_class_init,	/* class_init */
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GdaBatch),	/* instance_size */
			0,	/* n_preallocs */
			(GInstanceInitFunc) gda_batch_init,	/* instance_init */
			NULL,	/* value_table */
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaBatch",
					       &info, 0);
	}
	return type;
}
#else
guint
gda_batch_get_type (void)
{
	static guint gda_batch_type = 0;

	if (!gda_batch_type) {
		GtkTypeInfo gda_batch_info = {
			"GdaBatch",
			sizeof (GdaBatch),
			sizeof (GdaBatchClass),
			(GtkClassInitFunc) gda_batch_class_init,
			(GtkObjectInitFunc) gda_batch_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL,
		};
		gda_batch_type =
			gtk_type_unique (gtk_object_get_type (),
					 &gda_batch_info);
	}
	return gda_batch_type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_batch_class_init (GdaBatchClass * klass, gpointer data)
{
	/* FIXME: No signals yet in GObject */
	klass->begin_transaction = NULL;
	klass->commit_transaction = NULL;
	klass->rollback_transaction = NULL;
	klass->execute_command = NULL;
}
#else
static void
gda_batch_class_init (GdaBatchClass * klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	gda_batch_signals[GDA_BATCH_BEGIN_TRANSACTION] =
		gtk_signal_new ("begin_transaction",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GdaBatchClass,
						   begin_transaction),
				gtk_signal_default_marshaller, GTK_TYPE_NONE,
				0);
	gda_batch_signals[GDA_BATCH_COMMIT_TRANSACTION] =
		gtk_signal_new ("commit_transaction", GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GdaBatchClass,
						   commit_transaction),
				gtk_signal_default_marshaller, GTK_TYPE_NONE,
				0);
	gda_batch_signals[GDA_BATCH_ROLLBACK_TRANSACTION] =
		gtk_signal_new ("rollback_transaction", GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GdaBatchClass,
						   rollback_transaction),
				gtk_signal_default_marshaller, GTK_TYPE_NONE,
				0);
	gda_batch_signals[GDA_BATCH_EXECUTE_COMMAND] =
		gtk_signal_new ("execute_command", GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GdaBatchClass,
						   execute_command),
				gtk_marshal_NONE__POINTER, GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	gtk_object_class_add_signals (object_class, gda_batch_signals,
				      GDA_BATCH_LAST_SIGNAL);

	klass->begin_transaction = 0;
	klass->commit_transaction = 0;
	klass->rollback_transaction = 0;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_batch_init (GdaBatch * job, GdaBatchClass * klass)
#else
gda_batch_init (GdaBatch * job)
#endif
{
	g_return_if_fail (GDA_IS_BATCH (job));

	job->cnc = 0;
	job->transaction_mode = TRUE;
	job->is_running = FALSE;
	job->commands = 0;
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
#ifdef HAVE_GOBJECT
	return GDA_BATCH (g_object_new (GDA_TYPE_BATCH, NULL));
#else
	return GDA_BATCH (gtk_type_new (gda_batch_get_type ()));
#endif
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

	gda_batch_clear (job);
#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (job));
#else
	gtk_object_destroy (GTK_OBJECT (job));
#endif
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

	job->cnc = 0;
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
	    gda_connection_supports (job->cnc,
				     GDA_Connection_FEATURE_TRANSACTIONS)) {
		if (gda_connection_begin_transaction (job->cnc) == -1) {
			/* FIXME: emit "error" signal */
			return FALSE;
		}
#ifndef HAVE_GOBJECT		/* FIXME */
		gtk_signal_emit (GTK_OBJECT (job),
				 gda_batch_signals
				 [GDA_BATCH_BEGIN_TRANSACTION]);
#endif
	}

	/* traverse list of commands */
	job->is_running = TRUE;
	while (node && job->is_running) {
		gchar *cmd = (gchar *) node->data;
		if (cmd && strlen (cmd) > 0) {
			GdaRecordset *recset;
			gulong reccount;

			/* execute command */
#ifndef HAVE_GOBJECT		/* FIXME */
			gtk_signal_emit (GTK_OBJECT (job),
					 gda_batch_signals
					 [GDA_BATCH_EXECUTE_COMMAND], cmd);
#endif
			recset = gda_connection_execute (job->cnc, cmd,
							 &reccount, 0);
			if (recset) {
				gda_recordset_free (recset);
			}
			else {
				/* FIXME: emit "error" signal */
				if (job->transaction_mode &&
				    gda_connection_supports (job->cnc,
							     GDA_Connection_FEATURE_TRANSACTIONS))
				{
					/* rollback transaction */
					gda_connection_rollback_transaction
						(job->cnc);
#ifndef HAVE_GOBJECT		/* FIXME */
					gtk_signal_emit (GTK_OBJECT (job),
							 gda_batch_signals
							 [GDA_BATCH_ROLLBACK_TRANSACTION]);
#endif
					return FALSE;
				}
			}
		}
		node = g_list_next (node);
	}

	if (job->transaction_mode &&
	    gda_connection_supports (job->cnc,
				     GDA_Connection_FEATURE_TRANSACTIONS)) {
		if (gda_connection_commit_transaction (job->cnc) == -1) {
			/* FIXME: emit "error" signal */
			return FALSE;
		}
#ifndef HAVE_GOBJECT		/* FIXME */
		gtk_signal_emit (GTK_OBJECT (job),
				 gda_batch_signals
				 [GDA_BATCH_COMMIT_TRANSACTION]);
#endif
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
