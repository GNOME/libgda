#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include <libgda-ui/libgda-ui.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <sql-parser/gda-sql-parser.h>

#include <demos.h>

/* to be used by example */
GdaConnection *demo_cnc;
GdaSqlParser  *demo_parser;


static GtkTextBuffer *info_buffer;
static GtkTextBuffer *source_buffer;

static gchar *current_file = NULL;


enum {
	TITLE_COLUMN,
	FILENAME_COLUMN,
	FUNC_COLUMN,
	STYLE_COLUMN,
	NUM_COLUMNS
};

typedef struct _CallbackData CallbackData;
struct _CallbackData
{
	GtkTreeModel *model;
	GtkTreePath *path;
};

/**
 * demo_find_file:
 * @base: base filename
 * @err:  location to store error, or %NULL.
 * 
 * Looks for @base first in the current directory, then in the
 * location GTK+ where it will be installed on make install,
 * returns the first file found.
 * 
 * Returns: the filename, if found or %NULL
 **/
gchar *
demo_find_file (const char *base, GError **err)
{
	g_return_val_if_fail (err == NULL || *err == NULL, NULL);
  
	if (g_file_test ("demos.h", G_FILE_TEST_EXISTS) &&
	    g_file_test (base, G_FILE_TEST_EXISTS))
		return g_strdup (base);
	else {
		gchar *filename;

		filename = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "demo", base, NULL);
		if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
			g_set_error (err, G_FILE_ERROR, G_FILE_ERROR_NOENT,
				     "Cannot find demo data file \"%s\"", base);
			g_free (filename);
			return NULL;
		}
		return filename;
	}
}

static void
window_closed_cb (G_GNUC_UNUSED GtkWidget *window, gpointer data)
{
	CallbackData *cbdata = data;
	GtkTreeIter iter;
	PangoStyle style;

	gtk_tree_model_get_iter (cbdata->model, &iter, cbdata->path);
	gtk_tree_model_get (GTK_TREE_MODEL (cbdata->model), &iter,
			    STYLE_COLUMN, &style,
			    -1);
	if (style == PANGO_STYLE_ITALIC)
		gtk_tree_store_set (GTK_TREE_STORE (cbdata->model), &iter,
				    STYLE_COLUMN, PANGO_STYLE_NORMAL,
				    -1);

	gtk_tree_path_free (cbdata->path);
	g_free (cbdata);
}

gboolean
read_line (FILE *stream, GString *str)
{
	int n_read = 0;
  
#ifdef HAVE_FLOCKFILE
	flockfile (stream);
#endif

	g_string_truncate (str, 0);
  
	while (1) {
		int c;
		
#ifdef HAVE_FLOCKFILE
		c = getc_unlocked (stream);
#else
		c = getc (stream);
#endif
		
		if (c == EOF)
			goto done;
		else
			n_read++;
		
		switch (c) {
		case '\r':
		case '\n': {
#ifdef HAVE_FLOCKFILE
			int next_c = getc_unlocked (stream);
#else
			int next_c = getc (stream);
#endif
			
			if (!(next_c == EOF ||
			      (c == '\r' && next_c == '\n') ||
			      (c == '\n' && next_c == '\r')))
				ungetc (next_c, stream);
			
			goto done;
		}
		default:
			g_string_append_c (str, c);
		}
	}
	
 done:
	
#ifdef HAVE_FLOCKFILE
	funlockfile (stream);
#endif

	return n_read > 0;
}


/* Stupid syntax highlighting.
 *
 * No regex was used in the making of this highlighting.
 * It should only work for simple cases.  This is good, as
 * that's all we should have in the demos.
 */
/* This code should not be used elsewhere, except perhaps as an example of how
 * to iterate through a text buffer.
 */
enum {
	STATE_NORMAL,
	STATE_IN_COMMENT
};

static gchar *tokens[] =
	{
		"/*",
		"\"",
		NULL
	};

static gchar *types[] =
	{
		"static",
		"const ",
		"void",
		"gint",
		"int ",
		"char ",
		"gchar ",
		"gfloat",
		"float",
		"gint8",
		"gint16",
		"gint32",
		"guint",
		"guint8",
		"guint16",
		"guint32",
		"guchar",
		"glong",
		"gboolean" ,
		"gshort",
		"gushort",
		"gulong",
		"gdouble",
		"gldouble",
		"gpointer",
		"NULL",
		"GList",
		"GSList",
		"FALSE",
		"TRUE",
		"FILE ",
		"GtkObject ",
		"GtkColorSelection ",
		"GtkWidget ",
		"GtkButton ",
		"GdkColor ",
		"GdkRectangle ",
		"GdkEventExpose ",
		"GdkGC ",
		"GdkPixbufLoader ",
		"GdkPixbuf ",
		"GError",
		"size_t",
		NULL
	};

static gchar *control[] =
	{
		" if ",
		" while ",
		" else",
		" do ",
		" for ",
		"?",
		":",
		"return ",
		"goto ",
		NULL
	};
void
parse_chars (gchar     *text,
	     gchar    **end_ptr,
	     gint      *state,
	     gchar    **tag,
	     gboolean   start)
{
	gint i;
	gchar *next_token;

	/* Handle comments first */
	if (*state == STATE_IN_COMMENT)	{
		*end_ptr = strstr (text, "*/");
		if (*end_ptr) {
			*end_ptr += 2;
			*state = STATE_NORMAL;
			*tag = "comment";
		}
		return;
	}
	
	*tag = NULL;
	*end_ptr = NULL;

	/* check for comment */
	if (!strncmp (text, "/*", 2)) {
		*end_ptr = strstr (text, "*/");
		if (*end_ptr)
			*end_ptr += 2;
		else
			*state = STATE_IN_COMMENT;
		*tag = "comment";
		return;
	}

	/* check for preprocessor defines */
	if (*text == '#' && start) {
		*end_ptr = NULL;
		*tag = "preprocessor";
		return;
	}
	
	/* functions */
	if (start && * text != '\t' && *text != ' ' && *text != '{' && *text != '}') {
		if (strstr (text, "("))	{
			*end_ptr = strstr (text, "(");
			*tag = "function";
			return;
		}
	}
	/* check for types */
	for (i = 0; types[i] != NULL; i++)
		if (!strncmp (text, types[i], strlen (types[i]))) {
			*end_ptr = text + strlen (types[i]);
			*tag = "type";
			return;
		}
	
	/* check for control */
	for (i = 0; control[i] != NULL; i++)
		if (!strncmp (text, control[i], strlen (control[i]))) {
			*end_ptr = text + strlen (control[i]);
			*tag = "control";
			return;
		}

	/* check for string */
	if (text[0] == '"') {
		gint maybe_escape = FALSE;
		
		*end_ptr = text + 1;
		*tag = "string";
		while (**end_ptr != '\000') {
			if (**end_ptr == '\"' && !maybe_escape)	{
				*end_ptr += 1;
				return;
			}
			if (**end_ptr == '\\')
				maybe_escape = TRUE;
			else
				maybe_escape = FALSE;
			*end_ptr += 1;
		}
		return;
	}
	
	/* not at the start of a tag.  Find the next one. */
	for (i = 0; tokens[i] != NULL; i++) {
		next_token = strstr (text, tokens[i]);
		if (next_token) {
			if (*end_ptr)
				*end_ptr = (*end_ptr<next_token)?*end_ptr:next_token;
			else
				*end_ptr = next_token;
		}
	}
	
	for (i = 0; types[i] != NULL; i++) {
		next_token = strstr (text, types[i]);
		if (next_token)	{
			if (*end_ptr)
				*end_ptr = (*end_ptr<next_token)?*end_ptr:next_token;
			else
				*end_ptr = next_token;
		}
	}
	
	for (i = 0; control[i] != NULL; i++) {
		next_token = strstr (text, control[i]);
		if (next_token)	{
			if (*end_ptr)
				*end_ptr = (*end_ptr<next_token)?*end_ptr:next_token;
			else
				*end_ptr = next_token;
		}
	}
}

/* While not as cool as c-mode, this will do as a quick attempt at highlighting */
static void
fontify (void)
{
	GtkTextIter start_iter, next_iter, tmp_iter;
	gint state;
	gchar *text;
	gchar *start_ptr, *end_ptr;
	gchar *tag;

	state = STATE_NORMAL;

	gtk_text_buffer_get_iter_at_offset (source_buffer, &start_iter, 0);

	next_iter = start_iter;
	while (gtk_text_iter_forward_line (&next_iter))	{
		gboolean start = TRUE;
		start_ptr = text = gtk_text_iter_get_text (&start_iter, &next_iter);
		
		do {
			parse_chars (start_ptr, &end_ptr, &state, &tag, start);
			
			start = FALSE;
			if (end_ptr) {
				tmp_iter = start_iter;
				gtk_text_iter_forward_chars (&tmp_iter, end_ptr - start_ptr);
			}
			else 
				tmp_iter = next_iter;
			if (tag)
				gtk_text_buffer_apply_tag_by_name (source_buffer, tag, &start_iter, &tmp_iter);
			
			start_iter = tmp_iter;
			start_ptr = end_ptr;
		}
		while (end_ptr);
		
		g_free (text);
		start_iter = next_iter;
	}
}

void
load_file (const gchar *filename)
{
	FILE *file;
	GtkTextIter start, end;
	char *full_filename;
	GError *err = NULL;
	GString *buffer = g_string_new (NULL);
	int state = 0;
	gboolean in_para = 0;

	if (current_file && !strcmp (current_file, filename)) {
		g_string_free (buffer, TRUE);
		return;
	}

	g_free (current_file);
	current_file = g_strdup (filename);
  
	gtk_text_buffer_get_bounds (info_buffer, &start, &end);
	gtk_text_buffer_delete (info_buffer, &start, &end);

	gtk_text_buffer_get_bounds (source_buffer, &start, &end);
	gtk_text_buffer_delete (source_buffer, &start, &end);

	full_filename = demo_find_file (filename, &err);
	if (!full_filename) {
		g_warning ("%s", err->message);
		g_error_free (err);
		return;
	}

	file = g_fopen (full_filename, "r");

	if (!file)
		g_warning ("Cannot open %s: %s\n", full_filename, g_strerror (errno));

	g_free (full_filename);

	if (!file)
		return;

	gtk_text_buffer_get_iter_at_offset (info_buffer, &start, 0);
	while (read_line (file, buffer)) {
		gchar *p = buffer->str;
		gchar *q;
		gchar *r;
		
		switch (state) {
		case 0:
			/* Reading title */
			while (*p == '/' || *p == '*' || g_ascii_isspace (*p))
				p++;
			r = p;
			while (*r != '/' && strlen (r))
				r++;
			if (strlen (r) > 0)
				p = r + 1;
			q = p + strlen (p);
			while (q > p && g_ascii_isspace (*(q - 1)))
				q--;
			
			if (q > p) {
				glong len_chars = g_utf8_pointer_to_offset (p, q);
				
				end = start;
				
				g_assert (strlen (p) >= (gsize)(q - p));
				gtk_text_buffer_insert (info_buffer, &end, p, q - p);
				start = end;
				
				gtk_text_iter_backward_chars (&start, len_chars);
				gtk_text_buffer_apply_tag_by_name (info_buffer, "title", &start, &end);
				
				start = end;
				
				state++;
			}
			break;
			
		case 1:
			/* Reading body of info section */
			while (g_ascii_isspace (*p))
				p++;
			if (*p == '*' && *(p + 1) == '/') {
				gtk_text_buffer_get_iter_at_offset (source_buffer, &start, 0);
				state++;
			}
			else {
				gsize len;
				
				while (*p == '*' || g_ascii_isspace (*p))
					p++;
				
				len = strlen (p);
				while ((len > 0) && g_ascii_isspace (*(p + len - 1)))
					len--;
				
				if (len > 0) {
					if (in_para)
						gtk_text_buffer_insert (info_buffer, &start, " ", 1);
					
					g_assert (strlen (p) >= len);
					gtk_text_buffer_insert (info_buffer, &start, p, len);
					in_para = 1;
				}
				else {
					gtk_text_buffer_insert (info_buffer, &start, "\n", 1);
					in_para = 0;
				}
			}
			break;
			
		case 2:
			/* Skipping blank lines */
			while (g_ascii_isspace (*p))
				p++;
			if (*p)	{
				p = buffer->str;
				state++;
				/* Fall through */
			}
			else
				break;
			
		case 3:
			/* Reading program body */
			gtk_text_buffer_insert (source_buffer, &start, p, -1);
			gtk_text_buffer_insert (source_buffer, &start, "\n", 1);
			break;
		}
	}
	
	fontify ();
	
	g_string_free (buffer, TRUE);
}

void
row_activated_cb (GtkTreeView       *tree_view,
                  GtkTreePath       *path,
		  G_GNUC_UNUSED GtkTreeViewColumn *column)
{
	GtkTreeIter iter;
	PangoStyle style;
	GDoDemoFunc func;
	GtkWidget *window;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (tree_view);
  
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (model),
			    &iter,
			    FUNC_COLUMN, &func,
			    STYLE_COLUMN, &style,
			    -1);

	if (func) {
		gtk_tree_store_set (GTK_TREE_STORE (model),
				    &iter,
				    STYLE_COLUMN, (style == PANGO_STYLE_ITALIC ? PANGO_STYLE_NORMAL : PANGO_STYLE_ITALIC),
				    -1);
		window = (func) (gtk_widget_get_toplevel (GTK_WIDGET (tree_view)));
		
		if (window != NULL) {
			CallbackData *cbdata;
			
			cbdata = g_new (CallbackData, 1);
			cbdata->model = model;
			cbdata->path = gtk_tree_path_copy (path);
			
			g_signal_connect (window, "destroy",
					  G_CALLBACK (window_closed_cb), cbdata);
		}
	}
}

static void
selection_cb (GtkTreeSelection *selection,
	      GtkTreeModel     *model)
{
	GtkTreeIter iter;
	GValue *value;

	if (! gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	value = g_slice_new0(GValue);
	gtk_tree_model_get_value (model, &iter,
				  FILENAME_COLUMN,
				  value);
	if (g_value_get_string (value))
		load_file (g_value_get_string (value));
	g_value_unset (value);
	g_slice_free(GValue, value);
}

static GtkWidget *
create_text (GtkTextBuffer **buffer,
	     gboolean        is_source)
{
	GtkWidget *scrolled_window;
	GtkWidget *text_view;
	PangoFontDescription *font_desc;

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
					     GTK_SHADOW_IN);
  
	text_view = gtk_text_view_new ();
  
	*buffer = gtk_text_buffer_new (NULL);
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (text_view), *buffer);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);

	gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);
  
	if (is_source) {
		font_desc = pango_font_description_from_string ("monospace");
		gtk_widget_override_font (text_view, font_desc);
		pango_font_description_free (font_desc);
		
		gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view),
					     GTK_WRAP_NONE);
	}
	else {
		/* Make it a bit nicer for text. */
		gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view),
					     GTK_WRAP_WORD);
		gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (text_view),
						      2);
		gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (text_view),
						      2);
	}
	
	return scrolled_window;
}

static GtkWidget *
create_tree (void)
{
	GtkTreeSelection *selection;
	GtkCellRenderer *cell;
	GtkWidget *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeStore *model;
	GtkTreeIter iter;
	GtkWidget *box, *label, *scrolled_window;

	Demo *d = gdaui_demos;

	model = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_INT);
	tree_view = gtk_tree_view_new ();
	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (model));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
				     GTK_SELECTION_BROWSE);
	gtk_widget_set_size_request (tree_view, 200, -1);

	/* this code only supports 1 level of children. If we
	 * want more we probably have to use a recursing function.
	 */
	while (d->title) {
		Demo *children = d->children;
		
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
		
		gtk_tree_store_set (GTK_TREE_STORE (model),
				    &iter,
				    TITLE_COLUMN, d->title,
				    FILENAME_COLUMN, d->filename,
				    FUNC_COLUMN, d->func,
				    STYLE_COLUMN, PANGO_STYLE_NORMAL,
				    -1);
		
		d++;
		
		if (!children)
			continue;
		
		while (children->title)	{
			GtkTreeIter child_iter;
			
			gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &iter);
			
			gtk_tree_store_set (GTK_TREE_STORE (model),
							    &child_iter,
					    TITLE_COLUMN, children->title,
					    FILENAME_COLUMN, children->filename,
					    FUNC_COLUMN, children->func,
					    STYLE_COLUMN, PANGO_STYLE_NORMAL,
					    -1);
			
			children++;
		}
	}
	
	cell = gtk_cell_renderer_text_new ();

	column = gtk_tree_view_column_new_with_attributes ("Widget (double click for demo)",
							   cell,
							   "text", TITLE_COLUMN,
							   "style", STYLE_COLUMN,
							   NULL);
  
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
				     GTK_TREE_VIEW_COLUMN (column));

	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter);
	gtk_tree_selection_select_iter (GTK_TREE_SELECTION (selection), &iter);

	g_signal_connect (selection, "changed", G_CALLBACK (selection_cb), model);
	g_signal_connect (tree_view, "row-activated", G_CALLBACK (row_activated_cb), model);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
  				    
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);

	GtkWidget *hbox, *spin;
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new ("Widget (double click for demo)");
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	spin = gtk_spinner_new ();
	gtk_spinner_start (GTK_SPINNER (spin));
	gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, FALSE, 10);
	gtk_widget_show_all (hbox);

	box = gtk_notebook_new ();
	gtk_notebook_append_page (GTK_NOTEBOOK (box), scrolled_window, hbox);

	gtk_widget_grab_focus (tree_view);

	return box;
}

int
main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *notebook;
	GtkWidget *hbox;
	GtkWidget *tree;
	GError *error = NULL;
	gchar *full_filename, *dirname;
	gchar *cncstring;
	gchar *str;
	GdaMetaStore *mstore;

	/* Initialize i18n support */
	str = gda_gbr_get_file_path (GDA_LOCALE_DIR, NULL);
        bindtextdomain (GETTEXT_PACKAGE, str);
	g_free (str);

        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        /* Initialize libgda-ui & GTK+ */
	gtk_init (&argc, &argv);
	gdaui_init ();

	/* Initialize GdaConnection object */
	full_filename = demo_find_file ("demo_db.db", &error);
	if (!full_filename) {
		g_warning (_("Can't find demo database file: %s"), error->message);
		g_error_free (error);
		exit (1);
	}

	dirname = g_path_get_dirname (full_filename);
	cncstring = g_strdup_printf ("DB_DIR=%s;DB_NAME=demo_db", dirname);
	g_free (dirname);
	demo_cnc = gda_connection_open_from_string ("SQLite", cncstring, NULL, 0, &error);
	if (!demo_cnc) {
		g_warning (_("Error opening the connection for file '%s':\n%s\n"),
			   full_filename,
			   error && error->message ?  error->message : _("No detail"));
		g_error_free (error);
		exit (1);
	}
	g_free (full_filename);
	g_free (cncstring);

	/* Initialize meta store */
	full_filename = demo_find_file ("demo_meta.db", &error);
	if (full_filename)
		mstore = gda_meta_store_new_with_file (full_filename);
	else
		mstore = gda_meta_store_new (NULL);

	g_free (full_filename);
	g_object_set (G_OBJECT (demo_cnc), "meta-store", mstore, NULL);
        g_object_unref (mstore);
	if (! full_filename)
		gda_connection_update_meta_store (demo_cnc, NULL, NULL);

	/* set main context for the connection */
	GMainContext *context;
	context = g_main_context_ref_thread_default ();
	gda_connection_set_main_context (NULL, NULL, context);
	g_main_context_unref (context);
	g_object_set (demo_cnc, "execution-slowdown", 1000000, NULL);

	/* Initialize parser object */
	demo_parser = gda_connection_create_parser (demo_cnc);
	if (!demo_parser)
		demo_parser = gda_sql_parser_new ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), _("Libgda-ui Code Demos"));
	g_signal_connect (window, "destroy",
			  G_CALLBACK (gtk_main_quit), NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (window), hbox);

	tree = create_tree ();
	gtk_box_pack_start (GTK_BOX (hbox), tree, FALSE, FALSE, 0);

	notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (hbox), notebook, TRUE, TRUE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  create_text (&info_buffer, FALSE),
				  gtk_label_new_with_mnemonic ("_Info"));

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  create_text (&source_buffer, TRUE),
				  gtk_label_new_with_mnemonic ("_Source"));

	gtk_text_buffer_create_tag (info_buffer, "title",
				    "font", "Sans 18",
				    NULL);
	gtk_text_buffer_create_tag (source_buffer, "comment",
				    "foreground", "DodgerBlue",
				    NULL);
	gtk_text_buffer_create_tag (source_buffer, "type",
				    "foreground", "ForestGreen",
				    NULL);
	gtk_text_buffer_create_tag (source_buffer, "string",
				    "foreground", "RosyBrown",
				    "weight", PANGO_WEIGHT_BOLD,
				    NULL);
	gtk_text_buffer_create_tag (source_buffer, "control",
				    "foreground", "purple",
				    NULL);
	gtk_text_buffer_create_tag (source_buffer, "preprocessor",
				    "style", PANGO_STYLE_OBLIQUE,
				    "foreground", "burlywood4",
				    NULL);
	gtk_text_buffer_create_tag (source_buffer, "function",
				    "weight", PANGO_WEIGHT_BOLD,
				    "foreground", "DarkGoldenrod4",
				    NULL);
  
	gtk_window_set_default_size (GTK_WINDOW (window), 700, 700);
	gtk_widget_show_all (window);
  
	load_file (gdaui_demos[0].filename);


	if (! g_getenv ("NO_DEMO_NOTICE")) {
		GtkWidget *msg;
		full_filename = demo_find_file ("demo_db.db", NULL);
		msg = gtk_message_dialog_new_with_markup (GTK_WINDOW (window), GTK_DIALOG_MODAL,
							  GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
							  _("<b><big>Note:\n</big></b>Many of the demonstrated items use an opened connection to the SQLite using the '%s' file.\n\n"
							    "In the source code shown here, the <i>demo_cnc</i> and <i>demo_parser</i> objects are created by the framework and made available to all the demonstrated items.\n\n"
							    "To illustrate that calls are non blocking, there is a spinner at the top (which must never stop spinning), and a 1 second delay is added whenever the connection is used."), full_filename);
		g_free (full_filename);
		g_signal_connect_swapped (msg, "response",
					  G_CALLBACK (gtk_widget_destroy), msg);
		gtk_widget_show (msg);
	}

	gtk_main ();

	return 0;
}
