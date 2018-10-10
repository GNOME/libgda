/*
 * Copyright (C) 2010 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <string.h>
#include <libgda-ui/libgda-ui.h>

#ifdef GDA_DEBUG
static gboolean
texttags_equal (GtkTextTag *tag1, GtkTextTag *tag2)
{
	return TRUE;
}

static gboolean
textbuffers_equal (GtkTextBuffer *buffer1, GtkTextBuffer *buffer2, GError **error)
{
	GtkTextIter iter1, iter2;
	gunichar ch1, ch2;
	gtk_text_buffer_get_start_iter (buffer1, &iter1);
	gtk_text_buffer_get_start_iter (buffer2, &iter2);

	do {
		ch1 = gtk_text_iter_get_char (&iter1);
		ch2 = gtk_text_iter_get_char (&iter2);
		if (ch1 != ch2) {
			g_set_error (error, GDAUI_RT_EDITOR_ERROR, GDAUI_RT_EDITOR_GENERAL_ERROR, "difference at line %d, offset %d",
				     gtk_text_iter_get_line (&iter1),
				     gtk_text_iter_get_line_offset (&iter1));
			return FALSE;
		}
		if (ch1 == 0)
			break;
		gchar buf1[6];
		if ((g_unichar_to_utf8 (ch1, buf1) == 1) &&
		    (*buf1 == '\n')) {
			gtk_text_iter_forward_char (&iter1);
			gtk_text_iter_forward_char (&iter2);
			continue;
		}

		GSList *list1, *list2, *p1, *p2;
		list1 = gtk_text_iter_get_tags (&iter1);
		list2 = gtk_text_iter_get_tags (&iter2);
		for (p1 = list1; p1; p1 = p1->next) {
			for (p2 = list2; p2; p2 = p2->next) {
				if (texttags_equal (GTK_TEXT_TAG (p1->data),
						    GTK_TEXT_TAG (p2->data))) {
					list2 = g_slist_remove (list2, p2->data);
					break;
				}
			}
			if (!p2) {
				g_set_error (error, GDAUI_RT_EDITOR_ERROR, GDAUI_RT_EDITOR_GENERAL_ERROR, "Missing text tag in textbuffer2 "
					     "at line %d, offset %d",
					     gtk_text_iter_get_line (&iter1),
					     gtk_text_iter_get_line_offset (&iter1));
				return FALSE;
			}
		}
		for (p2 = list2; p2; p2 = p2->next) {
			for (p1 = list1; p1; p1 = p1->next) {
				if (texttags_equal (GTK_TEXT_TAG (p1->data), GTK_TEXT_TAG (p2->data)))
					break;
			}
			if (!p1) {
				g_set_error (error, GDAUI_RT_EDITOR_ERROR, GDAUI_RT_EDITOR_GENERAL_ERROR, "Missing text tag in textbuffer1 "
					     "at line %d, offset %d",
					     gtk_text_iter_get_line (&iter1),
					     gtk_text_iter_get_line_offset (&iter1));
				return FALSE;
			}
		}
		g_slist_free (list1);
		g_slist_free (list2);
		gtk_text_iter_forward_char (&iter1);
		gtk_text_iter_forward_char (&iter2);
	} while ((ch1 != 0) && (ch2 != 0));

	GtkTextIter end1, end2;
	gtk_text_buffer_get_end_iter (buffer1, &end1);
	gtk_text_buffer_get_end_iter (buffer2, &end2);
	if (gtk_text_iter_compare (&iter1, &end1)) {
		g_set_error (error, GDAUI_RT_EDITOR_ERROR, GDAUI_RT_EDITOR_GENERAL_ERROR, "%s", "textbuffer1 is shorter than textbuffer2");
		return FALSE;
	}
	if (gtk_text_iter_compare (&iter2, &end2)) {
		g_set_error (error, GDAUI_RT_EDITOR_ERROR, GDAUI_RT_EDITOR_GENERAL_ERROR, "%s", "textbuffer2 is shorter than textbuffer1");
		return FALSE;
	}
	return TRUE;
}
#endif

static void
copy_cb (GtkButton *button, GdauiRtEditor *from)
{
	GdauiRtEditor *rte;
	gchar *data;
	rte = g_object_get_data (G_OBJECT (button), "destrte");
	data = gdaui_rt_editor_get_contents (from);
	gdaui_rt_editor_set_contents (rte, data, -1);
	g_free (data);

#ifdef GDA_DEBUG
	GtkTextBuffer *b1, *b2;
	GError *lerror = NULL;
	g_object_get ((GObject*) from, "buffer", &b1, NULL);
	g_object_get ((GObject*) rte, "buffer", &b2, NULL);
	if (! textbuffers_equal (b1, b2, &lerror)) {
		g_warning ("ERROR: %s\n", lerror->message);
		g_clear_error (&lerror);
	}
	g_object_unref ((GObject*) b1);
	g_object_unref ((GObject*) b2);
#endif
}


static GtkWidget *
create_window (void)
{
	GtkWidget *p_win = NULL;
	GtkWidget *vbox = NULL;
	GtkWidget *rte = NULL;
	GtkWidget *button;

	/* Window */
	p_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title  (GTK_WINDOW (p_win), "GtkTextView & GtkTextTag");
	gtk_window_set_default_size (GTK_WINDOW (p_win), 640, 480);
	gtk_container_set_border_width (GTK_CONTAINER (p_win), 5);
	gtk_window_set_position (GTK_WINDOW (p_win), GTK_WIN_POS_CENTER);
	g_signal_connect (G_OBJECT (p_win), "destroy", gtk_main_quit, NULL);

	/* contents */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_add (GTK_CONTAINER (p_win), vbox);
	rte = gdaui_rt_editor_new ();
	gtk_box_pack_start (GTK_BOX (vbox), rte, TRUE, TRUE, 0);
	gdaui_rt_editor_set_contents (GDAUI_RT_EDITOR (rte), "No tags here..., ``Monospaced here``\n= Title level 1 =\n\n"
				  "blah **important**\n\n"
				  "== title level 2 ==\n\n"
				  "blah //italic// blah.\n"
				  "and ** BOLD!//both italic and bold// Bold!**\nNice Picture: [[[R2RrUAAABIwBAQABAAAAPAAAABMAAAAT6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urq6urqAAAAy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLAAAAy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLyMfJxMnCycnFyMfJyMfJy8vLy8vLy8vLy8vLy8vLy8vLAAAAy8vLy8vLy8vLy8vLy8vLy8vLy8vLwcK8lcOSWMFRQ8I5RLo6YrBcmK+Uv8m+y8vLy8vLy8vLy8vLAAAAwcK8y8vLy8vLy8vLy8vLy8vLsMivN9AuA+gACvMACPcACvMAA+gACLUAPps2tLisy8vLy8vLy8vLAAAAnpVNycnFy8vLy8vLy8vLpcmjE9cGAvsAE/4GE/4GE/4GE/4GE/4GFPQGAsUAHJAWtbOzy8vLy8vLAAAAtagDnZx1y8vLy8vLv8m+L9cmAvsAE/4GE/4GE/4GE/4GE/4GE/4GE/4GFPQGCLUAQo88wcK8y8vLAAAAzsEHlokgwcK8y8vLi8qHAvsAE/4GE/4GE/4GE/4GE/4GE/4GE/4GE/4GE/4GE9cGBpIAiKeEy8vLAAAA3dAEn5ERsLCXxMnCVtJSAvsAFf4IE/4GE/4GE/4GE/4GE/4GE/4GE/4GE/4GE+kGCKUCW5JUy8vLAAAA5NgHo5UGn6BysMivONQyCPcAFf4IE/4GE/4GE/4GE/4GE/4GE/4GE/4GE/4GE+kGCKUCS5FEy8vLAAAA3dAEo5UGnZx1sMivONQyCPcAE/4GE/4GE/4GE/4GE/4GE/4GE/4GE/4GE/4GEeIIC5gAS5FEy8vLAAAAx7kFlokgqqqLwcK8TsRKCvMAE/4GE/4GE/4GE/4GE/4GE/4GE/4GE/4GFPQGE9cGBpIAW5JUy8vLAAAAtagDh3oRt8W2y8vLfK53B9YAE/4GE/4GE/4GE/4GE/4GE/4GE/4GE/4GEeIIEbsEBX4BfJ53y8vLAAAAk4YHlIxSy8vLy8vLt8W2GqoPA+gAE/4GE/4GE/4GE/4GE/4GFPQGE+kGFMcHBpIAI3EdqrWoy8vLAAAAhnwtvLq7y8vLy8vLy8vLiKeECKUCB9YAE+kGFPQGFPQGE+kGE9cGEbsEC5gAEnENkaeOy8vLy8vLAAAAtLisy8vLy8vLy8vLy8vLyMfJfJ53LYImCKUCCLUAEbsECLUACKUCBX4BI3EdkaeOy8vLy8vLy8vLAAAAy8vLy8vLy8vLy8vLy8vLy8vLy8vLmK+UYJRaOYUyKnsiKnsiPXw1YYdbqrWoy8vLy8vLy8vLy8vLAAAAy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLtbOzqrWoq6eltLisyMfJy8vLy8vLy8vLy8vLy8vLAAAAy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8vLy8tYQwAA]]] Yes\n"
				      "- List item --One--\n"
				      "- \n"
				      "- List item **Two**\n"
				      " - sub1\n"
				      " - sub2\n\n"
				      "multi line markup **starting here\nand ending here**\n"
				      "multi line markup **starting here**\n**and ending here**\nnot here"
				      "A Line with **formatting -- error **\n",
				      -1);
	button = gtk_button_new_with_label ("Copy");
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 5);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (copy_cb), rte);

	rte = gdaui_rt_editor_new ();
	gtk_box_pack_start (GTK_BOX (vbox), rte, TRUE, TRUE, 0);
	gdaui_rt_editor_set_editable (GDAUI_RT_EDITOR (rte), FALSE);
	g_object_set_data (G_OBJECT (button), "destrte", rte);

	return p_win;
}

/*
 * Entree du programme:
 */
int main (int argc, char ** argv)
{
	gtk_init (& argc, & argv);
	gdaui_init ();

	/*
	 * Creation et mise en place de la fenetre:
	 */
	gtk_widget_show_all (create_window ());
	gtk_main ();

	return 0;
}
