/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "browser-canvas.h"
#include "browser-canvas-priv.h"
#include <cairo.h>
#include <goocanvas.h>

typedef struct {
	BrowserCanvas     *canvas;
	GtkPrintSettings *settings;
	GtkPageSetup     *page_setup;
	gboolean          show_page_numbers;

	gdouble           page_width, page_height;
	gint              h_npages, v_npages;
	gdouble           scale;
} PrintPageData;

static GObject *print_create_custom_widget_cb (GtkPrintOperation *operation, PrintPageData *pdata);
static void print_begin (GtkPrintOperation *operation, GtkPrintContext *context, PrintPageData *pdata);
static void print_end (GtkPrintOperation *operation, GtkPrintContext *context, PrintPageData *pdata);
static void print_draw_page (GtkPrintOperation *operation, GtkPrintContext *context, gint page_nr, PrintPageData *pdata);

static GtkPrintSettings *print_settings = NULL;
static GtkPageSetup *page_setup = NULL;
static gboolean show_page_numbers = TRUE;

/**
 * browser_canvas_print
 * @canvas: the #BrowserCanvas to print
 *
 * Prints @canvas using the GTK+ printing framework (displays printing options)
 */
void
browser_canvas_print (BrowserCanvas *canvas)
{
	GtkPrintOperation *print;
	GtkPrintOperationResult res;
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (canvas));
	PrintPageData *pdata;

	if (!print_settings)
		print_settings = gtk_print_settings_new  ();
	if (!page_setup)
		page_setup = gtk_page_setup_new ();

	pdata = g_new0 (PrintPageData, 1);
	pdata->canvas = canvas;
	pdata->settings = print_settings;
	pdata->page_setup = page_setup;
	pdata->show_page_numbers = show_page_numbers;

	print = gtk_print_operation_new ();
	
	gtk_print_operation_set_print_settings (print, print_settings);
	gtk_print_operation_set_default_page_setup (print, pdata->page_setup);
	
	g_signal_connect (print, "create-custom-widget", G_CALLBACK (print_create_custom_widget_cb), pdata);
	g_signal_connect (print, "begin_print", G_CALLBACK (print_begin), pdata);
	g_signal_connect (print, "end_print", G_CALLBACK (print_end), pdata);
	g_signal_connect (print, "draw_page", G_CALLBACK (print_draw_page), pdata);
	gtk_print_operation_set_custom_tab_label (print, _("Page size and zoom"));
	res = gtk_print_operation_run (print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				       (GtkWindow*) toplevel, NULL);
	
	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		g_object_unref (print_settings);
		print_settings = g_object_ref (gtk_print_operation_get_print_settings (print));
		if (page_setup != pdata->page_setup) {
			g_object_unref (page_setup);
			page_setup = pdata->page_setup;
		}
		show_page_numbers = pdata->show_page_numbers;
	}
	else if (page_setup != pdata->page_setup)
		g_object_unref (pdata->page_setup);

	g_object_unref (print);
	g_free (pdata);
}

static void
print_begin (GtkPrintOperation *operation, G_GNUC_UNUSED GtkPrintContext *context, PrintPageData *pdata)
{
	gtk_print_operation_set_n_pages (operation, pdata->h_npages * pdata->v_npages);
	gtk_print_operation_set_default_page_setup (operation, pdata->page_setup);
}

static void
print_end (G_GNUC_UNUSED GtkPrintOperation *operation, G_GNUC_UNUSED GtkPrintContext *context,
	   G_GNUC_UNUSED PrintPageData *pdata)
{
	
}

static void
print_draw_page (G_GNUC_UNUSED GtkPrintOperation *operation, GtkPrintContext *context, gint page_nr, PrintPageData *pdata)
{
	cairo_t *cr;
	GooCanvasBounds bounds, canvas_bounds;
	gint col_page, line_page;
#define DRAW_DEBUG
#undef DRAW_DEBUG

	cr = gtk_print_context_get_cairo_context (context);

	line_page = page_nr / pdata->h_npages;
	col_page = page_nr % pdata->h_npages;

	goo_canvas_item_get_bounds (goo_canvas_get_root_item (pdata->canvas->priv->goocanvas), &canvas_bounds);

	/*g_print ("Printing page col%d line%d\n", col_page, line_page);*/

#ifdef DRAW_DEBUG
	cairo_save (cr);
	/* X axis */
	cairo_set_source_rgba (cr, 0., 1., 0., 0.8);
	cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 10.);
	cairo_move_to (cr, 50., -2.);
	cairo_show_text (cr, "X");
	cairo_stroke (cr);

	cairo_move_to (cr, 0., 0.);
	cairo_line_to (cr, 50., 0.);
	cairo_rel_line_to (cr, -10., 10.);
	cairo_stroke (cr);
	cairo_move_to (cr, 50., 0.);
	cairo_rel_line_to (cr, -10., -10.);
	cairo_stroke (cr);

	/* Y axis */
	cairo_set_source_rgba (cr, 0., 0., 1., 0.8);

	cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 10.);
	cairo_move_to (cr, -12., 55.);
	cairo_show_text (cr, "Y");
	cairo_stroke (cr);

	cairo_move_to (cr, 0., 0.);
	cairo_line_to (cr, 0., 50.);
	cairo_rel_line_to (cr, 10., -10.);
	cairo_stroke (cr);
	cairo_move_to (cr, 0., 50.);
	cairo_rel_line_to (cr, -10., -10.);
	cairo_stroke (cr);

	cairo_rectangle (cr, 0, 0, gtk_print_context_get_width (context), gtk_print_context_get_height (context));
	cairo_set_source_rgba (cr, 0., 0., 0., 1.);
	cairo_set_line_width (cr, 0.5);
	gdouble dash_length = 2.;
	cairo_set_dash (cr, &dash_length, 1, 0.);
	cairo_stroke (cr);
	cairo_restore (cr);
#endif

	if (pdata->show_page_numbers) {
		cairo_text_extents_t extents;
		gchar *str = g_strdup_printf (_("Page %d of %d horizontally and %d of %d vertically"), 
					      col_page + 1, pdata->h_npages, 
					      line_page + 1, pdata->v_npages);
		cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size (cr, 10.);
		cairo_text_extents (cr, str, &extents);
		cairo_move_to (cr, gtk_print_context_get_width (context) - extents.width - extents.x_bearing, 
			       gtk_print_context_get_height (context) - extents.height - extents.y_bearing);
		cairo_show_text (cr, str);
		g_free (str);
		cairo_stroke (cr);
	}

	bounds.x1 = col_page * pdata->page_width + canvas_bounds.x1;
	bounds.x2 = bounds.x1 + pdata->page_width;
	bounds.y1 = line_page * pdata->page_height + canvas_bounds.y1;
	bounds.y2 = bounds.y1 + pdata->page_height;

	cairo_scale (cr, pdata->scale, pdata->scale);
	cairo_translate (cr, - bounds.x1, - bounds.y1);
	goo_canvas_render (pdata->canvas->priv->goocanvas, cr, &bounds, .8);
	/*
	g_print ("Scale %.2f, cairo's bounds: (%.2fx%.2f) => (%.2fx%.2f), canvas's bounds: (%.2fx%.2f) => (%.2fx%.2f)\n", 
		 pdata->scale, bounds.x1, bounds.y1, bounds.x2, bounds.y2,
		 canvas_bounds.x1, canvas_bounds.y1, canvas_bounds.x2, canvas_bounds.y2);
	*/
}

typedef struct {
	PrintPageData *pdata;
	GtkSpinButton *zoom;
	GtkSpinButton *h_npages;
	GtkSpinButton *v_npages;
} PrintCustomData;

static void print_page_setup_cb (GtkWidget *button, PrintCustomData *cdata);
static void print_h_npages_value_changed_cb (GtkSpinButton *entry, PrintCustomData *cdata);
static void print_v_npages_value_changed_cb (GtkSpinButton *entry, PrintCustomData *cdata);
static void print_zoom_value_changed_cb (GtkSpinButton *entry, PrintCustomData *cdata);
static void print_page_numbers_toggled_cb (GtkToggleButton *toggle, PrintCustomData *cdata);

static GObject *
print_create_custom_widget_cb (G_GNUC_UNUSED GtkPrintOperation *operation, PrintPageData *pdata)
{
	GtkWidget *vbox, *bbox, *button, *label, *hbox, *grid, *entry;
	PrintCustomData *cdata;

	cdata = g_new0 (PrintCustomData, 1);
	cdata->pdata = pdata;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);

	/* page size's adjustments */
	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);

	button = gtk_button_new_with_label (_("Adjust page's size and orientation"));
	g_signal_connect (button, "clicked", G_CALLBACK (print_page_setup_cb), cdata);
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

	/* zoom control */
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), _("<b>Zoom</b>"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 10);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        label = gtk_label_new ("    ");
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	grid = gtk_grid_new ();
	gtk_box_pack_start (GTK_BOX (hbox), grid, TRUE, TRUE, 0);

	label = gtk_label_new (_("Number of pages used:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

	entry = gtk_spin_button_new_with_range (1., 100., 1.);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (entry), 0);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);
	cdata->h_npages = (GtkSpinButton*) entry;
	g_signal_connect (entry, "value-changed",
			  G_CALLBACK (print_h_npages_value_changed_cb), cdata);

	label = gtk_label_new (_("horizontally"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 2, 0, 1, 1);

	entry = gtk_spin_button_new_with_range (1., 100., 1.);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (entry), 0);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 1, 1);
	cdata->v_npages = (GtkSpinButton*) entry;
	g_signal_connect (entry, "value-changed",
			  G_CALLBACK (print_v_npages_value_changed_cb), cdata);

	label = gtk_label_new (_("vertically"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 2, 1, 1, 1);

	label = gtk_label_new (_("Zoom factor:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

	entry = gtk_spin_button_new_with_range (.1, 10., .05);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (entry), 2);
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 2, 1, 1);
	cdata->zoom = (GtkSpinButton*) entry;
	g_signal_connect (entry, "value-changed",
			  G_CALLBACK (print_zoom_value_changed_cb), cdata);

	/* misc options */
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), _("<b>Page numbers</b>"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 10);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
        label = gtk_label_new ("    ");
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, FALSE, 0);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);

	button = gtk_check_button_new_with_label (_("Print page numbers"));
	g_signal_connect (button, "toggled", G_CALLBACK (print_page_numbers_toggled_cb), cdata);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), cdata->pdata->show_page_numbers);
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

	/* correct start state */
	gtk_widget_show_all (vbox);
	g_object_set_data_full (G_OBJECT (vbox), "cdata", cdata, g_free);

	/* default zoom to 1 */
	gtk_spin_button_set_value (cdata->zoom, 1.);

	return G_OBJECT (vbox);
}

static void
print_page_numbers_toggled_cb (GtkToggleButton *toggle, PrintCustomData *cdata)
{
	cdata->pdata->show_page_numbers = gtk_toggle_button_get_active (toggle);
}

static void
print_page_setup_cb (GtkWidget *button, PrintCustomData *cdata)
{
	GtkPageSetup *setup;
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));

	setup = cdata->pdata->page_setup;
	cdata->pdata->page_setup = gtk_print_run_page_setup_dialog ((GtkWindow *) toplevel, 
								    cdata->pdata->page_setup, cdata->pdata->settings);
	if ((cdata->pdata->page_setup != setup) && (setup != page_setup))
		g_object_unref (setup);

	print_zoom_value_changed_cb (cdata->zoom, cdata);
}

static void
print_h_npages_value_changed_cb (GtkSpinButton *entry, PrintCustomData *cdata)
{
	gdouble page_width, page_height;
	GooCanvasBounds bounds;
	gdouble canvas_height;
	gdouble zoom;
	gint h_npages, v_npages;

	h_npages = (gint) gtk_spin_button_get_value (entry);
	page_width = gtk_page_setup_get_page_width (cdata->pdata->page_setup, GTK_UNIT_POINTS);
	page_height = gtk_page_setup_get_page_height (cdata->pdata->page_setup, GTK_UNIT_POINTS);

	goo_canvas_item_get_bounds (goo_canvas_get_root_item (cdata->pdata->canvas->priv->goocanvas), &bounds);
	zoom = (gdouble) h_npages * page_width / (bounds.x2 - bounds.x1);

	canvas_height = (bounds.y2 - bounds.y1) * zoom;
	v_npages = (gint) (canvas_height / page_height + 1.);
	
	g_signal_handlers_block_by_func (cdata->zoom, G_CALLBACK (print_zoom_value_changed_cb), cdata);
	gtk_spin_button_set_value (cdata->zoom, zoom);
	g_signal_handlers_unblock_by_func (cdata->zoom, G_CALLBACK (print_zoom_value_changed_cb), cdata);
	g_signal_handlers_block_by_func (cdata->v_npages, G_CALLBACK (print_v_npages_value_changed_cb), cdata);
	gtk_spin_button_set_value (cdata->v_npages, v_npages);
	g_signal_handlers_unblock_by_func (cdata->v_npages, G_CALLBACK (print_v_npages_value_changed_cb), cdata);

	cdata->pdata->scale = zoom;
	cdata->pdata->page_width = page_width / zoom;
	cdata->pdata->page_height = page_height / zoom;
	cdata->pdata->h_npages = h_npages;
	cdata->pdata->v_npages = v_npages;

	/*
	g_print ("Pages: %d/%d Page:%.2f/%.2f\n", cdata->pdata->h_npages, cdata->pdata->v_npages,
		 cdata->pdata->page_width, cdata->pdata->page_height);
	*/
}

static void
print_v_npages_value_changed_cb (GtkSpinButton *entry, PrintCustomData *cdata)
{
	gdouble page_width, page_height;
	GooCanvasBounds bounds;
	gdouble canvas_width;
	gdouble zoom;
	gint h_npages, v_npages;

	v_npages = (gint) gtk_spin_button_get_value (entry);
	page_width = gtk_page_setup_get_page_width (cdata->pdata->page_setup, GTK_UNIT_POINTS);
	page_height = gtk_page_setup_get_page_height (cdata->pdata->page_setup, GTK_UNIT_POINTS);

	goo_canvas_item_get_bounds (goo_canvas_get_root_item (cdata->pdata->canvas->priv->goocanvas), &bounds);
	zoom = (gdouble) v_npages * page_height / (bounds.y2 - bounds.y1);

	canvas_width = (bounds.x2 - bounds.x1) * zoom;
	h_npages = (gint) (canvas_width / page_width + 1.);
	
	g_signal_handlers_block_by_func (cdata->zoom, G_CALLBACK (print_zoom_value_changed_cb), cdata);
	gtk_spin_button_set_value (cdata->zoom, zoom);
	g_signal_handlers_unblock_by_func (cdata->zoom, G_CALLBACK (print_zoom_value_changed_cb), cdata);
	g_signal_handlers_block_by_func (cdata->h_npages, G_CALLBACK (print_h_npages_value_changed_cb), cdata);
	gtk_spin_button_set_value (cdata->h_npages, h_npages);
	g_signal_handlers_unblock_by_func (cdata->h_npages, G_CALLBACK (print_h_npages_value_changed_cb), cdata);

	cdata->pdata->scale = zoom;
	cdata->pdata->page_width = page_width / zoom;
	cdata->pdata->page_height = page_height / zoom;
	cdata->pdata->h_npages = h_npages;
	cdata->pdata->v_npages = v_npages;

	/*
	g_print ("Pages: %d/%d Page:%.2f/%.2f\n", cdata->pdata->h_npages, cdata->pdata->v_npages,
		 cdata->pdata->page_width, cdata->pdata->page_height);
	*/
}

static void
print_zoom_value_changed_cb (GtkSpinButton *entry, PrintCustomData *cdata)
{
	gdouble page_width, page_height;
	GooCanvasBounds bounds;
	gdouble canvas_width, canvas_height;
	gdouble zoom;
	gint h_npages, v_npages;

	zoom = gtk_spin_button_get_value (entry);
	page_width = gtk_page_setup_get_page_width (cdata->pdata->page_setup, GTK_UNIT_POINTS);
	page_height = gtk_page_setup_get_page_height (cdata->pdata->page_setup, GTK_UNIT_POINTS);

	goo_canvas_item_get_bounds (goo_canvas_get_root_item (cdata->pdata->canvas->priv->goocanvas), &bounds);
	canvas_width = (bounds.x2 - bounds.x1) * zoom;
	canvas_height = (bounds.y2 - bounds.y1) * zoom;
	h_npages = (gint) (canvas_width / page_width + 1.);
	v_npages = (gint) (canvas_height / page_height + 1.);

	g_signal_handlers_block_by_func (cdata->h_npages, G_CALLBACK (print_h_npages_value_changed_cb), cdata);
	gtk_spin_button_set_value (cdata->h_npages, h_npages);
	g_signal_handlers_unblock_by_func (cdata->h_npages, G_CALLBACK (print_h_npages_value_changed_cb), cdata);
	g_signal_handlers_block_by_func (cdata->v_npages, G_CALLBACK (print_v_npages_value_changed_cb), cdata);
	gtk_spin_button_set_value (cdata->v_npages, v_npages);
	g_signal_handlers_unblock_by_func (cdata->v_npages, G_CALLBACK (print_v_npages_value_changed_cb), cdata);

	cdata->pdata->scale = zoom;
	cdata->pdata->page_width = page_width / zoom;
	cdata->pdata->page_height = page_height / zoom;
	cdata->pdata->h_npages = h_npages;
	cdata->pdata->v_npages = v_npages;

	/*
	g_print ("Pages: %d/%d Page:%.2f/%.2f\n", cdata->pdata->h_npages, cdata->pdata->v_npages,
		 cdata->pdata->page_width, cdata->pdata->page_height);
	*/
}

