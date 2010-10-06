#include <gtk/gtk.h>
#include <libgda-ui/data-entries/gdaui-entry.h>
#include <libgda-ui/data-entries/gdaui-formatted-entry.h>
#include <libgda-ui/data-entries/gdaui-numeric-entry.h>
#include <stdlib.h>
#include <string.h>

#define NB_ENTRIES 50
static GtkWidget *entries[NB_ENTRIES];

static void
print_unicode (const gchar *ptr)
{
	gunichar wc;
	wc = g_utf8_get_char_validated (ptr, -1);
	g_print ("%s <=> %" G_GUINT32_FORMAT ", IS_print: %d\n", ptr, wc, g_unichar_isprint (wc));
}

static void
entry_changed_cb (GdauiEntry *entry, const gchar *id)
{
	gchar *tmp;
	tmp = gdaui_entry_get_text (entry);
	g_print ("Entry %s changed to: [%s]", id, tmp);
	if (GDAUI_IS_FORMATTED_ENTRY (entry)) {
		g_free (tmp);
		tmp = gdaui_formatted_entry_get_text (GDAUI_FORMATTED_ENTRY (entry));
		g_print (" => [%s]", tmp);
	}
	g_print ("\n");
		
	g_free (tmp);
}

static void
prop_length_set_cb (G_GNUC_UNUSED GtkButton *button, GdauiEntry *entry)
{
	gchar *tmp;
	gint max;
	gsize i;
	tmp = gdaui_entry_get_text (entry);
	max = atoi (tmp);
	g_free (tmp);
	g_print ("Setting Max entries' length to %d\n", max);

	for (i = 0; i < NB_ENTRIES; i++)
		if (entries [i])
			gdaui_entry_set_max_length (GDAUI_ENTRY (entries [i]), max);
}

static void
prop_text_set_cb (G_GNUC_UNUSED GtkButton *button, GdauiEntry *entry)
{
	gchar *tmp;
	gsize i;
	tmp = gdaui_entry_get_text (entry);
	g_print ("Setting entries' text to [%s]\n", tmp);
	for (i = 0; i < NB_ENTRIES; i++)
		if (entries [i])
			gdaui_entry_set_text (GDAUI_ENTRY (entries [i]), tmp);
	g_free (tmp);
}

static void
prop_text_null_cb (G_GNUC_UNUSED GtkButton *button, G_GNUC_UNUSED gpointer data)
{
	gsize i;

	g_print ("Setting entries' text to NULL\n");
	for (i = 0; i < NB_ENTRIES; i++)
		if (entries [i])
			gdaui_entry_set_text (GDAUI_ENTRY (entries [i]), NULL);
}

GtkWidget *
make_label (gint index, const gchar *text)
{
	GtkWidget *label;
	gchar *tmp;
	tmp = g_strdup_printf ("#%d: %s", index, text ? text : "");
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	
	return label;
}

int
main (int argc, char* argv[])
{
	GtkWidget *entry, *label, *button;
	GtkWidget *window, *hbox, *vbox, *table, *top_vbox;
	gint index = 0;
	gchar *tmp;

	gtk_init (&argc, &argv);
	memset (entries, 0, sizeof (entries));

	print_unicode ("0");
	print_unicode ("9");
	print_unicode ("@");
	print_unicode ("^");
	print_unicode ("#");
	print_unicode ("€");


	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	top_vbox = gtk_vbox_new (FALSE, 0);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	gtk_container_add(GTK_CONTAINER(window), top_vbox);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (top_vbox), hbox, FALSE, FALSE, 0);

	/*
	 * GdauiEntry widgets
	 */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), "<b>GdauiEntry</b>");
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

#define HAVE_NORMAL
#ifdef HAVE_NORMAL
	/* #0 */
	tmp = g_strdup_printf ("#%d", index);
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_entry_new (NULL, NULL);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);
	/* #1 */
	tmp = g_strdup_printf ("#%d", index);
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_entry_new ("€ ", NULL);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

	/* #2 */
	tmp = g_strdup_printf ("#%d", index);
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_entry_new (NULL, " Ê");
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

	/* #3 */
	tmp = g_strdup_printf ("#%d", index);
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_entry_new ("€€ ", " êê");
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);
#endif

#define HAVE_FORMATTED
#ifdef HAVE_FORMATTED
	/*
	 * GdauiFormattedEntry widgets
	 */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), "<b>GdauiFormattedEntry</b>");
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	/* #4 */
	tmp = g_strdup_printf ("#%d", index);
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_formatted_entry_new ("TIME=00:00:00", NULL);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

	/* #5 */
	tmp = g_strdup_printf ("#%d", index);
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_formatted_entry_new ("TIME=00€00:00",
					   "------- --   ");
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);
#endif

#define HAVE_NUMERIC
#ifdef HAVE_NUMERIC
	/*
	 * GdauiNumericEntry widgets
	 */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), "<b>GdauiNumericEntry</b>");
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	/* #6 */
	tmp = g_strdup_printf ("#%d: %s", index, "G_TYPE_CHAR");
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_numeric_entry_new (G_TYPE_CHAR);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

	/* #7 */
	tmp = g_strdup_printf ("#%d: %s", index, "G_TYPE_UINT, thousand sep=','");
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_numeric_entry_new (G_TYPE_UINT);
	g_object_set (G_OBJECT (entry), "thousands-sep", ',', NULL);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

	/* #8 */
	tmp = g_strdup_printf ("#%d: %s", index, "G_TYPE_FLOAT, n_decimals=2");
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_numeric_entry_new (G_TYPE_FLOAT);
	g_object_set (G_OBJECT (entry), "n-decimals", 2, NULL);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

	/* #9 */
	tmp = g_strdup_printf ("#%d: %s", index, "G_TYPE_DOUBLE");
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_numeric_entry_new (G_TYPE_DOUBLE);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

	/* #10 */
	tmp = g_strdup_printf ("#%d: %s", index, "G_TYPE_FLOAT, thousand sep=' ', n_decimals=2");
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_numeric_entry_new (G_TYPE_FLOAT);
	g_object_set (G_OBJECT (entry), "n-decimals", 2, NULL);
	g_object_set (G_OBJECT (entry), "thousands-sep", ' ', NULL);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

#endif

#define HAVE_COMPLETE
#ifdef HAVE_COMPLETE
	/*
	 * complete tests
	 */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), "<b>Complete tests</b>");
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	/* #11 */
	tmp = g_strdup_printf ("#%d: %s", index, "2 decimals with EURO");
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_numeric_entry_new (G_TYPE_FLOAT);
	g_object_set (G_OBJECT (entry), "suffix", "€", "n-decimals", 2, NULL);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

	/* #12 */
	tmp = g_strdup_printf ("#%d: %s", index, "3 decimals between markers");
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_numeric_entry_new (G_TYPE_FLOAT);
	g_object_set (G_OBJECT (entry), "prefix", "[", "suffix", "]", "n-decimals", 3, NULL);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

	/* #13 */
	tmp = g_strdup_printf ("#%d: %s", index, "**.* between markers");
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_formatted_entry_new ("**.*", NULL);
	g_object_set (G_OBJECT (entry), "suffix", "]", "prefix", "[", NULL);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

	/* #14 */
	tmp = g_strdup_printf ("#%d: %s", index, "900//@@@//^^^/##** between markers");
	label = gtk_label_new (tmp);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	entry = gdaui_formatted_entry_new ("900//@@@//^^^/##**", NULL);
	g_object_set (G_OBJECT (entry), "suffix", "]", "prefix", "[", NULL);
	entries[index] = entry;
	index++;
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (entry_changed_cb), tmp);

#endif

	/* properties */
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), "<b>Common properties:</b>");
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (top_vbox), label, FALSE, FALSE, 10);
	table = gtk_table_new (2, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (top_vbox), table, TRUE, TRUE, 0);

	label = gtk_label_new ("MaxLen:");
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, 0, 0, 0, 0);
	entry = gdaui_entry_new (NULL, NULL); /* FIXME: use a gint data entry */
	gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 0, 1);
	button = gtk_button_new_with_label ("Set");
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 0, 1, 0, 0, 0, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (prop_length_set_cb), entry);

	label = gtk_label_new ("Force text:");
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, 0, 0, 0, 0);
	entry = gdaui_entry_new (NULL, NULL);
	gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 1, 2);
	button = gtk_button_new_with_label ("Set");
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 1, 2, 0, 0, 0, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (prop_text_set_cb), entry);

	label = gtk_label_new ("Force text to NULL:");
	gtk_table_attach (GTK_TABLE (table), label, 0, 2, 2, 3, 0, 0, 0, 0);
	button = gtk_button_new_with_label ("Set");
	gtk_table_attach (GTK_TABLE (table), button, 2, 3, 2, 3, 0, 0, 0, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (prop_text_null_cb), entry);


	gtk_widget_show_all(window);
	gtk_main();
	return 0;
}
