/* 
 * Copyright © 2000 Eazel, Inc.
 * Copyright © 2002-2004 Marco Pesenti Gritti
 * Copyright © 2004, 2006 Christian Persch
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * Ephy port by Marco Pesenti Gritti <marco@it.gnome.org>
 * 
 * $Id$
 */

#ifdef GSEAL_ENABLE
  #define KEEP_GSEAL_ENABLE
  #undef GSEAL_ENABLE
#endif
#include <gtk/gtk.h>
#ifdef KEEP_GSEAL_ENABLE
  #define GSEAL_ENABLE
  #undef KEEP_GSEAL_ENABLE
#endif
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "browser-spinner.h"

/* Spinner cache implementation */

#define BROWSER_TYPE_SPINNER_CACHE			(browser_spinner_cache_get_type())
#define BROWSER_SPINNER_CACHE(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), BROWSER_TYPE_SPINNER_CACHE, BrowserSpinnerCache))
#define BROWSER_SPINNER_CACHE_CLASS(klass) 	(G_TYPE_CHECK_CLASS_CAST((klass), BROWSER_TYPE_SPINNER_CACHE, BrowserSpinnerCacheClass))
#define BROWSER_IS_SPINNER_CACHE(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), BROWSER_TYPE_SPINNER_CACHE))
#define BROWSER_IS_SPINNER_CACHE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), BROWSER_TYPE_SPINNER_CACHE))
#define BROWSER_SPINNER_CACHE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), BROWSER_TYPE_SPINNER_CACHE, BrowserSpinnerCacheClass))

typedef struct _BrowserSpinnerCache	BrowserSpinnerCache;
typedef struct _BrowserSpinnerCacheClass	BrowserSpinnerCacheClass;
typedef struct _BrowserSpinnerCachePrivate	BrowserSpinnerCachePrivate;

struct _BrowserSpinnerCacheClass
{
	GObjectClass parent_class;
};

struct _BrowserSpinnerCache
{
	GObject parent_object;

	/*< private >*/
	BrowserSpinnerCachePrivate *priv;
};

#define BROWSER_SPINNER_CACHE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), BROWSER_TYPE_SPINNER_CACHE, BrowserSpinnerCachePrivate))

struct _BrowserSpinnerCachePrivate
{
	/* Hash table of GdkScreen -> BrowserSpinnerCacheData */
	GHashTable *hash;
};

typedef struct
{
	guint ref_count;
	GtkIconSize size;
	int width;
	int height;
	GdkPixbuf **animation_pixbufs;
	guint n_animation_pixbufs;
} BrowserSpinnerImages;

#define LAST_ICON_SIZE			GTK_ICON_SIZE_DIALOG + 1
#define SPINNER_ICON_NAME		"process-working"
#define SPINNER_FALLBACK_ICON_NAME	"gnome-spinner"
#define BROWSER_SPINNER_IMAGES_INVALID	((BrowserSpinnerImages *) 0x1)

typedef struct
{
	GdkScreen *screen;
	GtkIconTheme *icon_theme;
	BrowserSpinnerImages *images[LAST_ICON_SIZE];
} BrowserSpinnerCacheData;

static void browser_spinner_cache_class_init (BrowserSpinnerCacheClass *klass);
static void browser_spinner_cache_init	  (BrowserSpinnerCache *cache);

static GObjectClass *browser_spinner_cache_parent_class;

static GType
browser_spinner_cache_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (BrowserSpinnerCacheClass),
			NULL,
			NULL,
			(GClassInitFunc) browser_spinner_cache_class_init,
			NULL,
			NULL,
			sizeof (BrowserSpinnerCache),
			0,
			(GInstanceInitFunc) browser_spinner_cache_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "BrowserSpinnerCache",
					       &our_info, 0);
	}

	return type;
}

static BrowserSpinnerImages *
browser_spinner_images_ref (BrowserSpinnerImages *images)
{
	g_return_val_if_fail (images != NULL, NULL);

	images->ref_count++;

	return images;
}

static void
browser_spinner_images_unref (BrowserSpinnerImages *images)
{
	g_return_if_fail (images != NULL);

	images->ref_count--;
	if (images->ref_count == 0)
	{
		guint i;

		for (i = 0; i < images->n_animation_pixbufs; ++i)
		{
			g_object_unref (images->animation_pixbufs[i]);
		}
		g_free (images->animation_pixbufs);

		g_free (images);
	}
}

static void
browser_spinner_cache_data_unload (BrowserSpinnerCacheData *data)
{
	GtkIconSize size;
	BrowserSpinnerImages *images;

	g_return_if_fail (data != NULL);

	for (size = GTK_ICON_SIZE_INVALID; size < LAST_ICON_SIZE; ++size)
	{
		images = data->images[size];
		data->images[size] = NULL;

		if (images != NULL && images != BROWSER_SPINNER_IMAGES_INVALID)
		{
			browser_spinner_images_unref (images);
		}
	}
}

static GdkPixbuf *
extract_frame (GdkPixbuf *grid_pixbuf,
	       int x,
	       int y,
	       int size)
{
	GdkPixbuf *pixbuf;

	if (x + size > gdk_pixbuf_get_width (grid_pixbuf) ||
	    y + size > gdk_pixbuf_get_height (grid_pixbuf))
	{
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_subpixbuf (grid_pixbuf,
					   x, y,
					   size, size);
	g_return_val_if_fail (pixbuf != NULL, NULL);

	return pixbuf;
}

static GdkPixbuf *
scale_to_size (GdkPixbuf *pixbuf,
	       int dw,
	       int dh)
{
	GdkPixbuf *result;
	int pw, ph;

	g_return_val_if_fail (pixbuf != NULL, NULL);

	pw = gdk_pixbuf_get_width (pixbuf);
	ph = gdk_pixbuf_get_height (pixbuf);

	if (pw != dw || ph != dh)
	{
		result = gdk_pixbuf_scale_simple (pixbuf, dw, dh,
						  GDK_INTERP_BILINEAR);
		g_object_unref (pixbuf);
		return result;
	}

	return pixbuf;
}

static BrowserSpinnerImages *
browser_spinner_images_load (GdkScreen *screen,
			  GtkIconTheme *icon_theme,
			  GtkIconSize icon_size)
{
	BrowserSpinnerImages *images;
	GdkPixbuf *icon_pixbuf, *pixbuf;
	GtkIconInfo *icon_info = NULL;
	int grid_width, grid_height, x, y, requested_size, size, isw, ish, n;
	const char *icon;
	GSList *list = NULL, *l;

	if (!gtk_icon_size_lookup_for_settings (gtk_settings_get_for_screen (screen),
						icon_size, &isw, &ish)) goto loser;

	requested_size = MAX (ish, isw);

	/* Load the animation. The 'rest icon' is the 0th frame */
	icon_info = gtk_icon_theme_lookup_icon (icon_theme,
						SPINNER_ICON_NAME,
					        requested_size, 0);
	if (icon_info == NULL)
	{
		g_warning ("Throbber animation not found");
	
		/* If the icon naming spec compliant name wasn't found, try the old name */
		icon_info = gtk_icon_theme_lookup_icon (icon_theme,
							SPINNER_FALLBACK_ICON_NAME,
							requested_size, 0);
		if (icon_info == NULL)
		{
			g_warning ("Throbber fallback animation not found either");
			goto loser;
		}
	}
	g_assert (icon_info != NULL);

	size = gtk_icon_info_get_base_size (icon_info);
	icon = gtk_icon_info_get_filename (icon_info);
	if (icon == NULL) goto loser;

	icon_pixbuf = gdk_pixbuf_new_from_file (icon, NULL);
	gtk_icon_info_free (icon_info);
	icon_info = NULL;

	if (icon_pixbuf == NULL)
	{
		g_warning ("Could not load the spinner file");
		goto loser;
	}

	grid_width = gdk_pixbuf_get_width (icon_pixbuf);
	grid_height = gdk_pixbuf_get_height (icon_pixbuf);

	n = 0;
	for (y = 0; y < grid_height; y += size)
	{
		for (x = 0; x < grid_width ; x += size)
		{
			pixbuf = extract_frame (icon_pixbuf, x, y, size);

			if (pixbuf)
			{
				list = g_slist_prepend (list, pixbuf);
				++n;
			}
			else
			{
				g_warning ("Cannot extract frame (%d, %d) from the grid\n", x, y);
			}
		}
	}

	g_object_unref (icon_pixbuf);

	if (list == NULL) goto loser;
	g_assert (n > 0);

	if (size > requested_size)
	{
		for (l = list; l != NULL; l = l->next)
		{
			l->data = scale_to_size (l->data, isw, ish);
		}
	}

	/* Now we've successfully got all the data */
	images = g_new (BrowserSpinnerImages, 1);
	images->ref_count = 1;

	images->size = icon_size;
	images->width = images->height = requested_size;

	images->n_animation_pixbufs = n;
	images->animation_pixbufs = g_new (GdkPixbuf *, n);

	for (l = list; l != NULL; l = l->next)
	{
		g_assert (l->data != NULL);
		images->animation_pixbufs[--n] = l->data;
	}
	g_assert (n == 0);

	g_slist_free (list);

	return images;

loser:
	if (icon_info)
	{
		gtk_icon_info_free (icon_info);
	}
	g_slist_foreach (list, (GFunc) g_object_unref, NULL);

	return NULL;
}

static BrowserSpinnerCacheData *
browser_spinner_cache_data_new (GdkScreen *screen)
{
	BrowserSpinnerCacheData *data;

	data = g_new0 (BrowserSpinnerCacheData, 1);

	data->screen = screen;
	data->icon_theme = gtk_icon_theme_get_for_screen (screen);
	g_signal_connect_swapped (data->icon_theme, "changed",
				  G_CALLBACK (browser_spinner_cache_data_unload),
				  data);

	return data;
}

static void
browser_spinner_cache_data_free (BrowserSpinnerCacheData *data)
{
	g_return_if_fail (data != NULL);
	g_return_if_fail (data->icon_theme != NULL);

	g_signal_handlers_disconnect_by_func
		(data->icon_theme,
		 G_CALLBACK (browser_spinner_cache_data_unload), data);

	browser_spinner_cache_data_unload (data);

	g_free (data);
}

static BrowserSpinnerImages *
browser_spinner_cache_get_images (BrowserSpinnerCache *cache,
			       GdkScreen *screen,
			       GtkIconSize icon_size)
{
	BrowserSpinnerCachePrivate *priv = cache->priv;
	BrowserSpinnerCacheData *data;
	BrowserSpinnerImages *images;

	g_return_val_if_fail (icon_size >= 0 && icon_size < LAST_ICON_SIZE, NULL);

	/* Backward compat: "invalid" meant "native" size which doesn't exist anymore */
	if (icon_size == GTK_ICON_SIZE_INVALID)
	{
		icon_size = GTK_ICON_SIZE_DIALOG;
	}

	data = g_hash_table_lookup (priv->hash, screen);
	if (data == NULL)
	{
		data = browser_spinner_cache_data_new (screen);
		/* FIXME: think about what happens when the screen's display is closed later on */
		g_hash_table_insert (priv->hash, screen, data);
	}

	images = data->images[icon_size];
	if (images == BROWSER_SPINNER_IMAGES_INVALID)
	{
		/* Load failed, but don't try endlessly again! */
		return NULL;
	}

	if (images != NULL)
	{
		/* Return cached data */
		return browser_spinner_images_ref (images);
	}

	images = browser_spinner_images_load (screen, data->icon_theme, icon_size);

	if (images == NULL)
	{
		/* Mark as failed-to-load */
		data->images[icon_size] = BROWSER_SPINNER_IMAGES_INVALID;

		return NULL;
	}

	data->images[icon_size] = images;

	return browser_spinner_images_ref (images);
}

static void
browser_spinner_cache_init (BrowserSpinnerCache *cache)
{
	BrowserSpinnerCachePrivate *priv;

	priv = cache->priv = BROWSER_SPINNER_CACHE_GET_PRIVATE (cache);

	priv->hash = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					    NULL,
					    (GDestroyNotify) browser_spinner_cache_data_free);
}

static void
browser_spinner_cache_finalize (GObject *object)
{
	BrowserSpinnerCache *cache = BROWSER_SPINNER_CACHE (object); 
	BrowserSpinnerCachePrivate *priv = cache->priv;

	g_hash_table_destroy (priv->hash);

	G_OBJECT_CLASS (browser_spinner_cache_parent_class)->finalize (object);
}

static void
browser_spinner_cache_class_init (BrowserSpinnerCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	browser_spinner_cache_parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = browser_spinner_cache_finalize;

	g_type_class_add_private (object_class, sizeof (BrowserSpinnerCachePrivate));
}

static BrowserSpinnerCache *spinner_cache = NULL;

static BrowserSpinnerCache *
browser_spinner_cache_ref (void)
{
	if (spinner_cache == NULL)
	{
		BrowserSpinnerCache **cache_ptr;

		spinner_cache = g_object_new (BROWSER_TYPE_SPINNER_CACHE, NULL);
		cache_ptr = &spinner_cache;
		g_object_add_weak_pointer (G_OBJECT (spinner_cache),
					   (gpointer *) cache_ptr);

		return spinner_cache;
	}
		
	return g_object_ref (spinner_cache);
}

/* Spinner implementation */

#define SPINNER_TIMEOUT 125 /* ms */

#define BROWSER_SPINNER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), BROWSER_TYPE_SPINNER, BrowserSpinnerPriv))

struct _BrowserSpinnerPriv
{
	GtkIconTheme *icon_theme;
	BrowserSpinnerCache *cache;
	GtkIconSize size;
	BrowserSpinnerImages *images;
	guint current_image;
	guint timeout;
	guint timer_task;
	guint spinning : 1;
	guint need_load : 1;
};

static void browser_spinner_class_init (BrowserSpinnerClass *class);
static void browser_spinner_init	    (BrowserSpinner *spinner);

static GObjectClass *parent_class;

GType
browser_spinner_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (BrowserSpinnerClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) browser_spinner_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (BrowserSpinner),
			0, /* n_preallocs */
			(GInstanceInitFunc) browser_spinner_init
		};

		type = g_type_register_static (GTK_TYPE_WIDGET,
					       "BrowserSpinner",
					       &our_info, 0);
	}

	return type;
}

static gboolean
browser_spinner_load_images (BrowserSpinner *spinner)
{
	BrowserSpinnerPriv *priv = spinner->priv;

	if (priv->need_load)
	{
		priv->images =
			browser_spinner_cache_get_images
				(priv->cache,
				 gtk_widget_get_screen (GTK_WIDGET (spinner)),
				 priv->size);

		priv->current_image = 0; /* 'rest' icon */
		priv->need_load = FALSE;
	}

	return priv->images != NULL;
}

static void
browser_spinner_unload_images (BrowserSpinner *spinner)
{
	BrowserSpinnerPriv *priv = spinner->priv;

	if (priv->images != NULL)
	{
		browser_spinner_images_unref (priv->images);
		priv->images = NULL;
	}

	priv->current_image = 0;
	priv->need_load = TRUE;
}

static void
icon_theme_changed_cb (GtkIconTheme *icon_theme,
		       BrowserSpinner *spinner)
{
	browser_spinner_unload_images (spinner);
	gtk_widget_queue_resize (GTK_WIDGET (spinner));
}

static void
browser_spinner_init (BrowserSpinner *spinner)
{
	BrowserSpinnerPriv *priv;

	priv = spinner->priv = BROWSER_SPINNER_GET_PRIVATE (spinner);

#if GTK_CHECK_VERSION(2,18,0)
	gtk_widget_set_has_window (GTK_WIDGET (spinner), FALSE);
#else
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (spinner), GTK_NO_WINDOW);
#endif

	priv->cache = browser_spinner_cache_ref ();
	priv->size = GTK_ICON_SIZE_DIALOG;
	priv->spinning = FALSE;
	priv->timeout = SPINNER_TIMEOUT;
	priv->need_load = TRUE;
}

static int
browser_spinner_expose (GtkWidget *widget, GdkEventExpose *event)
{
	BrowserSpinner *spinner = BROWSER_SPINNER (widget);
	BrowserSpinnerPriv *priv = spinner->priv;
	BrowserSpinnerImages *images;
	GdkPixbuf *pixbuf;
	GdkGC *gc;
	int x_offset, y_offset, width, height;
	GdkRectangle pix_area, dest;
	gboolean drawable;

#if GTK_CHECK_VERSION(2,18,0)
	drawable = gtk_widget_is_drawable (widget);
#else
	drawable = GTK_WIDGET_DRAWABLE (widget);
#endif
	if (!drawable)
		return FALSE;

	if (priv->need_load && !browser_spinner_load_images (spinner))
		return FALSE;

	images = priv->images;
	if (!images)
		return FALSE;

	/* Otherwise |images| will be NULL anyway */
	g_assert (images->n_animation_pixbufs > 0);
		
	g_assert (priv->current_image >= 0 &&
		  priv->current_image < images->n_animation_pixbufs);

	pixbuf = images->animation_pixbufs[priv->current_image];

	g_assert (pixbuf != NULL);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	/* Compute the offsets for the image centered on our allocation */
	GtkAllocation alloc;
#if GTK_CHECK_VERSION(2,18,0)
	gtk_widget_get_allocation (widget, &alloc);
#else
	alloc = widget->allocation;
#endif
	x_offset = (alloc.width - width) / 2;
	y_offset = (alloc.height - height) / 2;
	pix_area.x = x_offset + alloc.x;
	pix_area.y = y_offset + alloc.y;

	pix_area.width = width;
	pix_area.height = height;

	if (!gdk_rectangle_intersect (&event->area, &pix_area, &dest))
		return FALSE;

	GdkWindow *win;
#if GTK_CHECK_VERSION(2,18,0)
	win = gtk_widget_get_window (widget);
#else
	win = widget->window;
#endif
	gc = gdk_gc_new (win);
	gdk_draw_pixbuf (win, gc, pixbuf,
			 dest.x - x_offset - alloc.x,
			 dest.y - y_offset - alloc.y,
			 dest.x, dest.y,
			 dest.width, dest.height,
			 GDK_RGB_DITHER_MAX, 0, 0);
	g_object_unref (gc);

	return FALSE;
}

static gboolean
bump_spinner_frame_cb (BrowserSpinner *spinner)
{
	BrowserSpinnerPriv *priv = spinner->priv;

	/* This can happen when we've unloaded the images on a theme
	 * change, but haven't been in the queued size request yet.
	 * Just skip this update.
	 */
	if (priv->images == NULL) return TRUE;

	priv->current_image++;
	if (priv->current_image >= priv->images->n_animation_pixbufs)
	{
		/* the 0th frame is the 'rest' icon */
		priv->current_image = MIN (1, priv->images->n_animation_pixbufs);
	}

	gtk_widget_queue_draw (GTK_WIDGET (spinner));

	/* run again */
	return TRUE;
}

/**
 * browser_spinner_start:
 * @spinner: a #BrowserSpinner
 *
 * Start the spinner animation.
 **/
void
browser_spinner_start (BrowserSpinner *spinner)
{
	BrowserSpinnerPriv *priv = spinner->priv;

	priv->spinning = TRUE;

	if (GTK_WIDGET_MAPPED (GTK_WIDGET (spinner)) && /* GTK3 FIXME */
	    priv->timer_task == 0 &&
	    browser_spinner_load_images (spinner)) {
		/* the 0th frame is the 'rest' icon */
		priv->current_image = MIN (1, priv->images->n_animation_pixbufs);

		priv->timer_task =
			g_timeout_add_full (G_PRIORITY_LOW,
			                    priv->timeout,
				            (GSourceFunc) bump_spinner_frame_cb,
				            spinner,
				            NULL);
	}
}

static void
browser_spinner_remove_update_callback (BrowserSpinner *spinner)
{
	BrowserSpinnerPriv *priv = spinner->priv;

	if (priv->timer_task != 0) {
		g_source_remove (priv->timer_task);
		priv->timer_task = 0;
	}
}

/**
 * browser_spinner_stop:
 * @spinner: a #BrowserSpinner
 *
 * Stop the spinner animation.
 **/
void
browser_spinner_stop (BrowserSpinner *spinner)
{
	BrowserSpinnerPriv *priv = spinner->priv;

	priv->spinning = FALSE;
	priv->current_image = 0;

	if (priv->timer_task != 0) {
		browser_spinner_remove_update_callback (spinner);

		if (GTK_WIDGET_MAPPED (GTK_WIDGET (spinner))) /* GTK3 FIXME */
			gtk_widget_queue_draw (GTK_WIDGET (spinner));
	}
}

/*
 * browser_spinner_set_size:
 * @spinner: a #BrowserSpinner
 * @size: the size of type %GtkIconSize
 *
 * Set the size of the spinner.
 **/
void
browser_spinner_set_size (BrowserSpinner *spinner,
			  GtkIconSize size)
{
	if (size == GTK_ICON_SIZE_INVALID)
	{
		size = GTK_ICON_SIZE_DIALOG;
	}

	if (size != spinner->priv->size)
	{
		browser_spinner_unload_images (spinner);

		spinner->priv->size = size;

		gtk_widget_queue_resize (GTK_WIDGET (spinner));
	}
}

#if 0
/*
 * browser_spinner_set_timeout:
 * @spinner: a #BrowserSpinner
 * @timeout: time delay between updates to the spinner.
 *
 * Sets the timeout delay for spinner updates.
 **/
void
browser_spinner_set_timeout (BrowserSpinner *spinner,
			  guint timeout)
{
	BrowserSpinnerPriv *priv = spinner->priv;

	if (timeout != priv->timeout)
	{
		browser_spinner_stop (spinner);

		priv->timeout = timeout;

		if (priv->spinning)
		{
			browser_spinner_start (spinner);
		}
	}
}
#endif

static void
browser_spinner_size_request (GtkWidget *widget,
			   GtkRequisition *requisition)
{
	BrowserSpinner *spinner = BROWSER_SPINNER (widget);
	BrowserSpinnerPriv *priv = spinner->priv;

	if ((priv->need_load &&
	     !browser_spinner_load_images (spinner)) ||
            priv->images == NULL)
	{
		requisition->width = requisition->height = 0;
		gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (widget),
						   priv->size,
						   &requisition->width,
					           &requisition->height);
		return;
	}

	requisition->width = priv->images->width;
	requisition->height = priv->images->height;

	/* FIXME fix this hack */
	/* allocate some extra margin so we don't butt up against toolbar edges */
	if (priv->size != GTK_ICON_SIZE_MENU)
	{
		requisition->width += 2;
		requisition->height += 2;
	}
}

static void
browser_spinner_map (GtkWidget *widget)
{
	BrowserSpinner *spinner = BROWSER_SPINNER (widget);
	BrowserSpinnerPriv *priv = spinner->priv;

	GTK_WIDGET_CLASS (parent_class)->map (widget);

	if (priv->spinning)
	{
		browser_spinner_start (spinner);
	}
}

static void
browser_spinner_unmap (GtkWidget *widget)
{
	BrowserSpinner *spinner = BROWSER_SPINNER (widget);

	browser_spinner_remove_update_callback (spinner);

	GTK_WIDGET_CLASS (parent_class)->unmap (widget);
}

static void
browser_spinner_dispose (GObject *object)
{
	BrowserSpinner *spinner = BROWSER_SPINNER (object);

	g_signal_handlers_disconnect_by_func
			(spinner->priv->icon_theme,
		 G_CALLBACK (icon_theme_changed_cb), spinner);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
browser_spinner_finalize (GObject *object)
{
	BrowserSpinner *spinner = BROWSER_SPINNER (object);

	browser_spinner_remove_update_callback (spinner);
	browser_spinner_unload_images (spinner);

	g_object_unref (spinner->priv->cache);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
browser_spinner_screen_changed (GtkWidget *widget,
			     GdkScreen *old_screen)
{
	BrowserSpinner *spinner = BROWSER_SPINNER (widget);
	BrowserSpinnerPriv *priv = spinner->priv;
	GdkScreen *screen;

	if (GTK_WIDGET_CLASS (parent_class)->screen_changed)
	{
		GTK_WIDGET_CLASS (parent_class)->screen_changed (widget, old_screen);
	}

	screen = gtk_widget_get_screen (widget);

	/* FIXME: this seems to be happening when then spinner is destroyed!? */
	if (old_screen == screen) return;

	/* We'll get mapped again on the new screen, but not unmapped from
	 * the old screen, so remove timeout here.
	 */
	browser_spinner_remove_update_callback (spinner);

	browser_spinner_unload_images (spinner);

	if (old_screen != NULL)
	{
		g_signal_handlers_disconnect_by_func
			(gtk_icon_theme_get_for_screen (old_screen),
			 G_CALLBACK (icon_theme_changed_cb), spinner);
	}

	priv->icon_theme = gtk_icon_theme_get_for_screen (screen);
	g_signal_connect (priv->icon_theme, "changed",
			  G_CALLBACK (icon_theme_changed_cb), spinner);
}

static void
browser_spinner_class_init (BrowserSpinnerClass *class)
{
	GObjectClass *object_class =  G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = browser_spinner_dispose;
	object_class->finalize = browser_spinner_finalize;

	widget_class->expose_event = browser_spinner_expose;
	widget_class->size_request = browser_spinner_size_request;
	widget_class->map = browser_spinner_map;
	widget_class->unmap = browser_spinner_unmap;
	widget_class->screen_changed = browser_spinner_screen_changed;

	g_type_class_add_private (object_class, sizeof (BrowserSpinnerPriv));
}

/*
 * browser_spinner_new:
 *
 * Create a new #BrowserSpinner. The spinner is a widget
 * that gives the user feedback about network status with
 * an animated image.
 *
 * Return Value: the spinner #GtkWidget
 **/
GtkWidget *
browser_spinner_new (void)
{
	return GTK_WIDGET (g_object_new (BROWSER_TYPE_SPINNER, NULL));
}
