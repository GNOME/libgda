/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999 Rodrigo Moya
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

#include "config.h"
#include "gda-recordset.h"

#ifndef HAVE_GOBJECT
#  include <gtk/gtksignal.h>
#endif

enum {
	RECORDSET_ERROR,
	RECORDSET_EOF,
	RECORDSET_BOF,
	RECORDSET_ROW_CHANGED,
	LAST_SIGNAL
};

static gint gda_recordset_signals[LAST_SIGNAL] = {0, };

#ifdef HAVE_GOBJECT
static void   gda_recordset_class_init (GdaRecordsetClass *klass,
                                        gpointer data);
static void   gda_recordset_init       (GdaRecordset *rs,
                                        GdaRecordsetClass *klass);
#else
static void   gda_recordset_class_init (GdaRecordsetClass *);
static void   gda_recordset_init       (GdaRecordset *);
static void   gda_recordset_destroy    (GtkObject *object, gpointer user_data);
#endif

static void   gda_recordset_real_error (GdaRecordset *, GList *);
static gulong fetch_and_store          (GdaRecordset* rs, gint count, gpointer bookmark);
static gulong fetch_and_dont_store     (GdaRecordset* rs, gint count, gpointer bookmark);

/* per default there's no upper limit on the number of rows which can
 * be fetched from the data source
 */
#define GDA_DEFAULT_MAXROWS 0

/* per default fetch 64 rows at a time */
#define GDA_DEFAULT_CACHESIZE 64

static void
gda_recordset_real_error (GdaRecordset *rs, GList *error_list)
{
	GdaConnection *cnc;
	
	g_return_if_fail(GDA_IS_CONNECTION(cnc));
	
	if ((cnc = gda_recordset_get_connection(rs))) {
		gda_connection_add_error_list(cnc, error_list);
	}
}

#ifdef HAVE_GOBJECT
GType
gda_recordset_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		GTypeInfo info = {
			sizeof (GdaRecordsetClass),               /* class_size */
			NULL,                                      /* base_init */
			NULL,                                      /* base_finalize */
			(GClassInitFunc) gda_recordset_class_init, /* class_init */
			NULL,                                      /* class_finalize */
			NULL,                                      /* class_data */
			sizeof (GdaRecordset),                    /* instance_size */
			0,                                         /* n_preallocs */
			(GInstanceInitFunc) gda_recordset_init,    /* instance_init */
			NULL,                                      /* value_table */
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaRecordset", &info, 0);
	}
	return type;
}
#else
guint
gda_recordset_get_type (void)
{
	static guint gda_recordset_type = 0;
	
	if (!gda_recordset_type) {
		GtkTypeInfo gda_recordset_info = {
			"GdaRecordset",
			sizeof(GdaRecordset),
			sizeof(GdaRecordsetClass),
			(GtkClassInitFunc) gda_recordset_class_init,
			(GtkObjectInitFunc) gda_recordset_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL
		};
		gda_recordset_type = gtk_type_unique(gtk_object_get_type(), &gda_recordset_info);
	}
	return (gda_recordset_type);
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_recordset_class_init (GdaRecordsetClass *klass, gpointer data)
{
	/* FIXME: No signals in GObject yet */
	klass->error = gda_recordset_real_error;
	klass->eof = NULL;
	klass->bof = NULL;
}
#else
typedef void (*GtkSignal_NONE__INT_POINTER)(GtkObject,
                                            guint  arg1,
                                            gpointer arg2,
                                            gpointer user_data);

static void
gda_recordset_class_init (GdaRecordsetClass *klass)
{
	GtkObjectClass* object_class = (GtkObjectClass *) klass;

	gda_recordset_signals[RECORDSET_ERROR] =
		gtk_signal_new("error",
		               GTK_RUN_LAST,
		               object_class->type,
		               GTK_SIGNAL_OFFSET(GdaRecordsetClass, error),
		               gtk_marshal_NONE__POINTER,
		               GTK_TYPE_NONE,
		               1,
		               GTK_TYPE_POINTER);
	gda_recordset_signals[RECORDSET_EOF] =
		gtk_signal_new("eof",
		               GTK_RUN_LAST,
		               object_class->type,
		               GTK_SIGNAL_OFFSET(GdaRecordsetClass, eof),
		               gtk_signal_default_marshaller,
		               GTK_TYPE_NONE, 0);
	gda_recordset_signals[RECORDSET_BOF] =
		gtk_signal_new("bof",
		               GTK_RUN_LAST,
		               object_class->type,
		               GTK_SIGNAL_OFFSET(GdaRecordsetClass, bof),
		               gtk_signal_default_marshaller,
		               GTK_TYPE_NONE, 0);
	gda_recordset_signals[RECORDSET_ROW_CHANGED] =
		gtk_signal_new("row_changed",
		               GTK_RUN_LAST,
		               object_class->type,
		               GTK_SIGNAL_OFFSET(GdaRecordsetClass, row_changed),
		               gtk_signal_default_marshaller,
		               GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals(object_class, gda_recordset_signals, LAST_SIGNAL);
	
	klass->error = gda_recordset_real_error;
	klass->eof = NULL;
	klass->bof = NULL;
	klass->row_changed = NULL;
	object_class->destroy = gda_recordset_destroy;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_recordset_init (GdaRecordset *rs, GdaRecordsetClass *klass)
{
#else
gda_recordset_init (GdaRecordset *rs) {
#endif
	rs->external_cmd     = 0;
	rs->internal_cmd     = 0;
	rs->corba_rs         = CORBA_OBJECT_NIL;
	rs->cnc              = 0;
	rs->chunks           = 0;
	rs->chunks_length    = 0;
	rs->current_row      = 0;
	rs->field_attributes = 0;
	rs->max_index        = 0;
	rs->maxrows          = 0;
	rs->affected_rows    = 0;
	rs->cachesize        = 0;
	rs->open             = 0;
	rs->eof              = 1;
	rs->bof              = 1;
	rs->readonly         = 1;
	rs->cursor_location  = GDA_USE_CLIENT_CURSOR;
	rs->cursor_type      = GDA_OPEN_FWDONLY;
	rs->name             = 0;
}

#ifndef HAVE_GOBJECT
static void
gda_recordset_destroy (GtkObject *object, gpointer user_data)
{
	GdaRecordset *rs = (GdaRecordset *) object;
	GtkObjectClass *parent_class;

	g_return_if_fail(GDA_IS_RECORDSET(rs));

	if (rs->open)
		gda_recordset_close(rs);
	if (rs->internal_cmd) {
		gda_command_free(rs->internal_cmd);
	}

	parent_class = gtk_type_class(gtk_object_get_type());
	if (parent_class && parent_class->destroy)
		parent_class->destroy(object);
}
#endif

static void
free_chunks (GList* chunks)
{
	GDA_Row* row;
	GList*   ptr;
	
	ptr = chunks;
	while(ptr) {
		row = ptr->data;
		CORBA_free(row);
		ptr = g_list_next(ptr);
	}
}

static GDA_Row*
row_by_idx (GdaRecordset* rs, gint idx)
{
	GDA_Recordset_Chunk* chunk;
	
	if (idx <= 0 || idx > g_list_length(rs->chunks))
		return 0;
	chunk = g_list_nth(rs->chunks, idx-1)->data;
	
	return &chunk->_buffer[0];
}

/**
 * gda_recordset_new:
 *
 * Allocates space for a new recordset.
 *
 * Returns: the allocated recordset object
 */
GdaRecordset *
gda_recordset_new (void)
{
#ifdef HAVE_GOBJECT
	return GDA_RECORDSET (g_object_new (GDA_TYPE_RECORDSET, NULL));
#else
	return GDA_RECORDSET (gtk_type_new(gda_recordset_get_type()));
#endif
}

/**
 * gda_recordset_free:
 * @rs: the recordset which should be destroyed.
 *
 * This function frees all memory allocated by the recordset and
 * destroys all associations with commands and connections.
 *
 */
void
gda_recordset_free (GdaRecordset* rs)
{
	g_return_if_fail(GDA_IS_RECORDSET(rs));

#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (rs));
#else
	gtk_object_unref (GTK_OBJECT (rs));
#endif
}

/**
 * gda_recordset_close:
 * @rs: the recordset to close
 *
 * This function closes the recordset and frees the memory occupied by 
 * the actual data items. The recordset can be opened
 * again, doing the same query as before.
 * It is guaranteeed that the data cached by the recordset is
 * refetched from the server. Use this function is some
 * characteristics of the recordset must be changed
 *
 */
void
gda_recordset_close (GdaRecordset* rs)
{
	CORBA_Environment ev;
	
	g_return_if_fail(GDA_IS_RECORDSET(rs));
	
	CORBA_exception_init(&ev);
	if (!rs->open)
		return;
	rs->open = 0;
	if (rs->corba_rs) {
		GDA_Recordset_close(rs->corba_rs, &ev);
		gda_connection_corba_exception(rs->cnc, &ev);
	}
	rs->corba_rs = CORBA_OBJECT_NIL;
	if (rs->chunks) {
		free_chunks(rs->chunks);
	}
	rs->chunks = 0;
}

/**
 * gda_recordset_bof:
 * @rs: recordset to be checked
 *
 * This function is used to check if the recordset cursor is beyond
 * the first row.
 * If this function returns %TRUE any of the functions which actually returns
 * the value of a field an error is returned because the cursor
 * doesn't point to a row. 
 * Returns: TRUE if the cursor is beyond the first record or the
 * recordset is empty, FALSE otherwise.
 */
gboolean
gda_recordset_bof(GdaRecordset* rs)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), 0);
	
	if (!rs->open || !rs->current_row || rs->bof)
		return TRUE;
	return FALSE;
}

/**
 * gda_recordset_eof:
 * @rs:  recordset to be checked
 *
 * This function is used to check if the recordset cursor is after
 * the last row. 
 * If this function returns %TRUE any of the functions which actually returns
 * the value of a field an error is returned because the cursor
 * doesn't point to a row. 
 * Returns: TRUE if the cursor is after the last record or the
 * recordset is empty, FALSE otherwise.
 */
gboolean
gda_recordset_eof (GdaRecordset* rs)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), 0);
	
	if (!rs->open || rs->eof)
		return TRUE;
	return FALSE;
}

static gulong
fetch_and_store(GdaRecordset* rs, gint count, gpointer bookmark)
{
	GDA_Recordset_Chunk* chunk;
	gulong               current_idx;
	CORBA_Environment    ev;
	gint                 current_count;
	
	current_idx = rs->current_index + count;
	if (rs->chunks && current_idx <= rs->max_index && current_idx >= 0) {
		/* don't care about direction, it's in our cache */
		rs->bof = rs->eof = 0;
		rs->current_index = current_idx;
		rs->current_row = row_by_idx(rs, rs->current_index);
		return rs->current_index;
	}
	
	if (count < 0) {
		/* going backward  and underflow condition */
		rs->eof = 0;
		rs->bof = 1;
		return rs->current_index;
	}
	
	rs->eof = 0; /* be optimistic */
	rs->bof = 0;
	
	/* going forward. We have to fetch it from the provider */
	CORBA_exception_init(&ev);
	for (current_count = 0; current_count < count; current_count++) {
		chunk = GDA_Recordset_fetch(rs->corba_rs, 1, &ev);
		if (gda_connection_corba_exception(rs->cnc, &ev)) {
			/* An error */
			return rs->current_index;
		}
		if (chunk->_length == 0) {
			/* EOF. No more records there */
			CORBA_free(chunk);
			rs->current_index = rs->current_index + current_count;
			rs->max_index = rs->current_index;
			rs->eof = 1;
			if (!rs->current_row)
				rs->bof = 1;
			return rs->current_index;
		}
		rs->chunks = g_list_append(rs->chunks, chunk);
		if (gda_recordset_eof(rs)) {
			current_idx--;
		}
	}
	rs->max_index = current_idx;
	rs->current_index = current_idx;
	rs->current_row = row_by_idx(rs, rs->current_index);
	return rs->current_index;
}
  
static gulong
fetch_and_dont_store(GdaRecordset* rs, gint count, gpointer bookmark)
{
	GDA_Recordset_Chunk* chunk;
	gulong               current_idx;
	CORBA_Environment    ev;
	GList*               error_list;
	
	current_idx = rs->current_index;
	current_idx += count;
	if (current_idx < 0)
		return GDA_RECORDSET_INVALID_POSITION;
	
	CORBA_exception_init(&ev);
	GDA_Recordset_moveFirst(rs->corba_rs, &ev);
	error_list = gda_error_list_from_exception(&ev);
	if (error_list) {
		gda_connection_add_error_list(rs->cnc, error_list);
		return GDA_RECORDSET_INVALID_POSITION;
	}
	
	GDA_Recordset_move(rs->corba_rs, current_idx-1, 0, &ev);
	error_list = gda_error_list_from_exception(&ev);
	if (error_list) {
		gda_connection_add_error_list(rs->cnc, error_list);
		return GDA_RECORDSET_INVALID_POSITION;
	}
	chunk = GDA_Recordset_fetch(rs->corba_rs, count, &ev);
	error_list = gda_error_list_from_exception(&ev);
	if (error_list) {
		gda_connection_add_error_list(rs->cnc, error_list);
		return GDA_RECORDSET_INVALID_POSITION;
	}
	
	if (chunk->_length == 0) return GDA_RECORDSET_INVALID_POSITION;
	rs->current_index = current_idx;
	if (rs->chunks) {
		CORBA_free(rs->chunks->data);
		g_list_free(rs->chunks);
		rs->chunks = 0;
	}
	rs->chunks = g_list_append(rs->chunks, chunk);
	rs->current_row = row_by_idx(rs, rs->current_index);
	return rs->current_index;
}

/**
 * gda_recordset_move:
 * @rs: the recordset
 * @count: The number of records to skip
 * @bookmark: if not NULL, the cursor is positioned relative to the
 * record described by this paramter. seee
 * gda_recordset_get_bookmark() how to get bookmark values for
 * records.
 *
 * Moves the cursor of the recordset forward or backward. @count is
 * the number of records to move. If @count is negative the cursor is
 * moved towards the beginning. The function causes the recordset to
 * actually fetch records from the data source. Each fetch
 * from the data source fetches #cachesize rows in one turn. A maximum 
 * of #maxrows rows can be fetched. 
 *
 * If the cursor is on the second row and the @count parameter is -10, 
 * then the cursor is position in front of the first record
 * available. gda_rcordset_bof() will return %TRUE and 
 * the return value of the function is two, because the cursor actually
 * moved two records. 
 *
 * Returns: the number of the record the cursor is addressing after
 * the move or %GDA_RECORDSET_INVALID_POSITION if there was an error
 * fetching the rows.  
 */
gulong
gda_recordset_move (GdaRecordset* rs, gint count, gpointer bookmark)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), GDA_RECORDSET_INVALID_POSITION);
	g_return_val_if_fail(rs->corba_rs != NULL, GDA_RECORDSET_INVALID_POSITION);
	g_return_val_if_fail(rs->open, GDA_RECORDSET_INVALID_POSITION);
	
	if (rs->cursor_type == GDA_OPEN_FWDONLY && count < 0)
		return rs->current_index;
	
	if (count == 0)
		return rs->current_index;
	
	if (rs->cursor_location == GDA_USE_CLIENT_CURSOR)
		fetch_and_store(rs, count, 0);
	else fetch_and_dont_store(rs, count, 0);
	
	/* emit "row_changed" signal for data-bound widgets */
#ifndef HAVE_GOBJECT   /* FIXME */
	gtk_signal_emit(GTK_OBJECT(rs), gda_recordset_signals[RECORDSET_ROW_CHANGED]);
#endif
	return rs->current_index;
}

/**
 * gda_recordset_move_first:
 * @rs: the recordset
 *
 * Moves the cursor of the recordset to the first record. 
 * 
 * If the cursor is already on the the first record nothing happen.
 *
 * Returns: the position of the cursor, or
 * %GDA_RECORDSET_INVALID_POSITION
 * if there was an error. 
 */
gulong
gda_recordset_move_first (GdaRecordset* rs)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), GDA_RECORDSET_INVALID_POSITION);
	
	if (rs->cursor_location == GDA_USE_CLIENT_CURSOR) {
		GDA_Recordset_Chunk* chunk;
		if (!rs->chunks)
			return gda_recordset_move(rs, 1, 0);
		rs->current_index = 1;
		chunk = rs->chunks->data;
		rs->current_row = &chunk->_buffer[0];
		rs->eof = 0;
		return 0;
	}
	gda_log_error("gda_recordset_move_first for server based cursor NIY");
	return GDA_RECORDSET_INVALID_POSITION;
}

/**
 * gda_recordset_move_last:
 * @rs: the recordset
 *
 * Moves the cursor of the recordset to the last record. 
 * 
 * If the cursor is already on the the last record nothing happen.
 *
 * Returns: the position of the cursor, or
 * %GDA_RECORDSET_INVALID_POSITION
 * if there was an error. 
 */
gulong
gda_recordset_move_last (GdaRecordset* rs)
{
	gulong pos;
	gulong rc;
	
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), GDA_RECORDSET_INVALID_POSITION);
	
	if (rs->cursor_location == GDA_USE_CLIENT_CURSOR) {
		pos = rs->current_index;
		while (1) {
			rc = gda_recordset_move(rs, 1, 0);
			
			if (rc == GDA_RECORDSET_INVALID_POSITION)
				return GDA_RECORDSET_INVALID_POSITION;
			if (gda_recordset_eof(rs))
				return pos;
			pos++;
		}
	}
	gda_log_error("gda_recordset_move_first for server based cursor NIY");
	return GDA_RECORDSET_INVALID_POSITION;
}

/**
 * gda_recordset_move_next:
 * @rs: the recordset
 *
 * Moves the cursor of the recordset to the next record. 
 * 
 * Has the same effect as calling gda_recordset_move() with @count set to 1.
 *
 * Returns: the position of the cursor, or
 * %GDA_RECORDSET_INVALID_POSITION
 * if there was an error. 
 */
gulong
gda_recordset_move_next (GdaRecordset* rs)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), GDA_RECORDSET_INVALID_POSITION);
	return gda_recordset_move(rs, 1, 0);
}

/**
 * gda_recordset_move_prev
 * @rs: the recordset
 */
gulong
gda_recordset_move_prev (GdaRecordset *rs)
{
	return -1;
}

/**
 * gda_recordset_field_idx:
 * @rs: the recordset
 * @idx: the index of the field in the current row
 *
 * Returns a pointer to the field at position @idx of
 * the current row of the recordset.
 *
 * Returns: a pointer to a field structor or NULL if 
 * @idx is out of range
 */
GdaField*
gda_recordset_field_idx (GdaRecordset* rs, gint idx)
{
	gint       rowsize;
	GdaField* rc;
	
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), 0);
	g_return_val_if_fail(idx >= 0, 0);
	g_return_val_if_fail(rs->open, 0);
	
	if (!rs->current_row && !rs->field_attributes) {
		g_warning("This shouldn't happen. Inconsistent recordset\n");
		return 0;
	}
	rowsize = rs->field_attributes->_length;
	if (idx >= rowsize)
		return 0;
	rc = gda_field_new();
	rc->attributes = &rs->field_attributes->_buffer[idx];
	if (rs->current_row) {
		rc->real_value =  &rs->current_row->_buffer[idx].realValue;
		rc->shadow_value = &rs->current_row->_buffer[idx].shadowValue;
		rc->original_value = &rs->current_row->_buffer[idx].originalValue;
	}
	return rc;
}

/**
 * gda_recordset_field_name:
 * @rs: the recordset
 * @idx: the name of the field in the current row
 *
 * Returns a pointer to the field with the name  @name of
 * the current row of the recordset.
 *
 * Returns: a pointer to a field structor or NULL if 
 * @idx is out of range
 */
GdaField *
gda_recordset_field_name (GdaRecordset* rs, gchar* name)
{
	gint rowsize;
	gint idx;
	
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), 0);
	g_return_val_if_fail(rs->open, 0);
	
	if (!rs->current_row && !rs->field_attributes) {
		g_warning("This shouldn't happen. Inconsistent recordset\n");
		return 0;
	}
	rowsize = rs->field_attributes->_length;
	for (idx = 0; idx < rowsize; idx++) {
		if (strcasecmp(rs->field_attributes->_buffer[idx].name, name) == 0)
			return gda_recordset_field_idx(rs, idx);
	}
	return 0;
}

/**
 * gda_recordset_rowsize:
 * @rs: the recordset
 *
 * Returns the number of fields in a row of the current recordset
 */
gint
gda_recordset_rowsize (GdaRecordset* rs)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), -1);
	g_return_val_if_fail(rs->open, -1);
	
	if (!rs->current_row && !rs->field_attributes) {
		g_warning("This shouldn't happen. Inconsistent recordset\n");
		return 0;
	}
	return rs->field_attributes->_length;
}

/**
 * gda_recordset_affected_rows
 * @rs: the recordset
 *
 * Return the number of affected rows in the recordset
 */
gulong
gda_recordset_affected_rows (GdaRecordset *rs)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), 0);
	return rs->affected_rows;
}

/**
 * gda_recordset_set_connection:
 * @rs: the recordset
 * @cnc: the conneciton
 *
 * Associates a recordset with a connection. This function must not be 
 * called on a already opened  recordst.
 * This function must be called before any of the the
 * gda_recordset_open_txt() or gda_recordset_open_cmd() functions is
 * called.
 *
 * Returns: 0 if everyhting is okay, -1 on error
 */
gint
gda_recordset_set_connection (GdaRecordset* rs, GdaConnection* cnc)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), -1);
	
	if (rs->open) return -1;
	rs->cnc = cnc;
	return 0;
}

/**
 * gda_recordset_get_connection:
 * @rs: the rcordset
 *
 * Returns: the connection object used with this recordset. NULL if
 * the recordset isn't open or there's no connection assoctiated with
 * this recordset.
 */
GdaConnection*
gda_recordset_get_connection (GdaRecordset* rs)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), NULL);
	
	if (!rs->open)
		return NULL;
	return rs->cnc;
}

/**
 * gda_recordset_open:
 * @rs: the recordset
 * @cmd: the command object
 * @cursor_type: the type of the cursor used. Currently only a forward 
 * only cursor is implemented
 * @lock_type: the type of locks used for the command. Currently only
 * readonly type of locking is supported.
 * @options: currently not implemented. Used to identify how to parse
 * the @txt.
 *
 * This function opens a recordset. The recordset is filled with the
 * output of @cmd. Before this function is called various parameters
 * of the recordst might be changed. This is the most featurefull
 * function for retrieving results from a database.
 *
 * Returns: 0 if everytrhing is okay, -1 on error
 */
gint
gda_recordset_open (GdaRecordset* rs,
                    GdaCommand* cmd,
                    GDA_CursorType cursor_type,
                    GDA_LockType lock_type,
                    gulong options)
{
	CORBA_Environment    ev;
	GDA_CmdParameterSeq* corba_parameters;
	CORBA_long           affected = 0;
	GList*               error_list;
	
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), -1);
	g_return_val_if_fail(!rs->open, -1);

#ifdef HAVE_GOBJECT
	gda_recordset_init (rs, NULL);    /* FIXME: calling the constructor here */
#else                               /* is not very beautiful */
	gda_recordset_init (rs);
#endif
	corba_parameters = __gda_command_get_params (cmd);
	rs->cursor_type = cursor_type;
	rs->lock_type   = lock_type;
	CORBA_exception_init (&ev);
	rs->cnc = cmd->connection;
	rs->corba_rs = GDA_Command_open(cmd->command, corba_parameters,
	                                cursor_type, lock_type, &affected, &ev);
	error_list = gda_error_list_from_exception(&ev);
	if (error_list) {
		rs->corba_rs = 0;
		gda_connection_add_error_list(rs->cnc, error_list);
		return -1;
	}
	
	if (CORBA_Object_is_nil(rs->corba_rs, &ev)) {
		error_list = gda_error_list_from_exception(&ev);
		if (error_list) gda_connection_add_error_list(rs->cnc, error_list);
		rs->field_attributes = NULL;
		return -1;
	}
	else
		rs->field_attributes = GDA_Recordset_describe(rs->corba_rs, &ev);
	error_list = gda_error_list_from_exception(&ev);
	if (error_list) {
		gda_connection_add_error_list(rs->cnc, error_list);
		return -1;
	}
	rs->open = 1;
	rs->affected_rows = affected;
	return 0;
}

/**
 * gda_recordset_open_txt:
 * @rs: the recordset
 * @txt: the command text
 * @cursor_type: the type of the cursor used. Currently only a forward 
 * only cursor is implemented
 * @lock_type: the type of locks used for the command. Currently only
 * readonly type of locking is supported.
 * @options: currently not implemented. Used to identify how to parse
 * the @txt.
 *
 * This function opens a recordset. The recordset is filled with the
 * output of @txt. Before this function is called various parameters
 * of the recordst might be changed. This is the most featurefull
 * function for retrieving results from a database.
 *
 * Returns: 0 if everytrhing is okay, -1 on error
 */
gint
gda_recordset_open_txt (GdaRecordset* rs,
                        gchar* txt,
                        GDA_CursorType cursor_type,
                        GDA_LockType lock_type,
                        gulong options)
{
	CORBA_Environment  ev;
	GdaCommand*       cmd;
	gint               rc;
	
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), -1);
	
	if (rs->open) return -1;
	
	rs->cursor_type = cursor_type;
	rs->lock_type   = lock_type;
	CORBA_exception_init(&ev);
	cmd = gda_command_new();
	gda_command_set_connection(cmd, rs->cnc);
	gda_command_set_text(cmd, txt);
	rc = gda_recordset_open(rs, cmd, cursor_type, lock_type, options);
	gda_command_free(cmd);
	return rc;
}

/**
 * gda_recordset_add_field
 */
gint
gda_recordset_add_field (GdaRecordset* rs, GdaField* field)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), -1);
	g_return_val_if_fail(GDA_IS_FIELD(field), -1);
	g_return_val_if_fail(field->attributes, -1);
	g_return_val_if_fail(field->attributes->name, -1);
	
	if (rs->field_attributes) {
		GDA_FieldAttributes* old;
		gint                 idx;
		
		for (idx = 0; idx < rs->field_attributes->_length; idx++) {
			if (strcasecmp(rs->field_attributes->_buffer[idx].name,
			               field->attributes->name) == 0)
				return -1;
		}
		rs->field_attributes->_length++;
		old = rs->field_attributes->_buffer;
		rs->field_attributes->_buffer = CORBA_sequence_GDA_FieldAttributes_allocbuf(rs->field_attributes->_length);
		memcpy(rs->field_attributes->_buffer, old, rs->field_attributes->_length - 1);
	}
	else {
		rs->field_attributes->_length = 1;
		rs->field_attributes->_buffer = CORBA_sequence_GDA_FieldAttributes_allocbuf(rs->field_attributes->_length);
	}
	rs->field_attributes->_buffer[rs->field_attributes->_length-1].definedSize = field->attributes->definedSize;
	rs->field_attributes->_buffer[rs->field_attributes->_length-1].name = CORBA_string_dup(field->attributes->name);
	rs->field_attributes->_buffer[rs->field_attributes->_length-1].scale = field->attributes->scale;
	rs->field_attributes->_buffer[rs->field_attributes->_length-1].gdaType = field->attributes->gdaType;
	rs->field_attributes->_buffer[rs->field_attributes->_length-1].cType = field->attributes->cType;
	return 0;
}

/**
 * gda_recordset_get_cursorloc:
 * @rs: the recordset
 *
 * Returns: the current value of the cursor location attribute
 */
GDA_CursorLocation
gda_recordset_get_cursorloc (GdaRecordset* rs)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), -1);
	return rs->cursor_location;
}

/**
 * gda_recordset_set_cursorloc:
 * @rs: the recordset
 * @loc: the cursor location
 *
 * Set the cursor location attribute to @loc.
 */

void
gda_recordset_set_cursorloc(GdaRecordset* rs, GDA_CursorLocation loc)
{
	g_return_if_fail(GDA_IS_RECORDSET(rs));
	rs->cursor_location = loc;
}

/**
 * gda_recordset_get_cursortype:
 * @rs: the recordset
 *
 * Returns: the current value of the cursor type attriburte
 */
GDA_CursorType
gda_recordset_get_cursortype(GdaRecordset* rs)
{
	g_return_val_if_fail(GDA_IS_RECORDSET(rs), -1);
	return rs->cursor_type;
}

/**
 * gda_recordset_set_cursortype:
 * @rs: the recordset
 * @type: the cursor type
 *
 * Set the cursor type attribute to @type.
 */
void
gda_recordset_set_cursortype(GdaRecordset* rs, GDA_CursorType type)
{
	g_return_if_fail(GDA_IS_RECORDSET(rs));
	rs->cursor_type = type;
}
