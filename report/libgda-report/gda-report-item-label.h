/* GDA report libary
 * Copyright (C) 1998-2003 The GNOME Foundation.
 *
 * AUTHORS:
 *	Santi Camps <santi@gnome-db.org>
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

#if !defined(__gda_report_item_label_h__)
#  define __gda_report_item_label_h__

#include <glib-object.h>
#include <libgda-report/gda-report-item.h>
#include <libgda-report/gda-report-types.h>


G_BEGIN_DECLS

#define GDA_REPORT_TYPE_ITEM_LABEL		(gda_report_item_label_get_type())
#define GDA_REPORT_ITEM_LABEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_REPORT_TYPE_ITEM_LABEL, GdaReportItemLabel))
#define GDA_REPORT_ITEM_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST (klass, GDA_REPORT_TYPE_ITEM_LABEL, GdaReportItemLabelClass))
#define GDA_REPORT_IS_ITEM_LABEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_REPORT_TYPE_ITEM_LABEL))
#define GDA_REPORT_IS_ITEM_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GDA_REPORT_TYPE_ITEM_LABEL))

typedef struct _GdaReportItemLabel GdaReportItemLabel;
typedef struct _GdaReportItemLabelClass GdaReportItemLabelClass;

struct _GdaReportItemLabel {
	GdaReportItem object;
};

struct _GdaReportItemLabelClass {
	GdaReportItemClass parent_class;
};


GType gda_report_item_label_get_type (void) G_GNUC_CONST;

GdaReportItem *gda_report_item_label_new (GdaReportValid *valid,
					  const gchar *id);

GdaReportItem *gda_report_item_label_new_from_dom (xmlNodePtr node);

xmlNodePtr gda_report_item_label_to_dom (GdaReportItem *item);

gboolean gda_report_item_label_remove (GdaReportItem *item);

gboolean gda_report_item_label_set_id (GdaReportItem *item,
				       const gchar *value);

gchar *gda_report_item_label_get_id (GdaReportItem *item);

gboolean gda_report_item_label_set_active (GdaReportItem *item,
					  const gchar *value);

gchar *gda_report_item_label_get_active (GdaReportItem *item);

gboolean gda_report_item_label_set_visible (GdaReportItem *item,
					   const gchar *value);

gchar *gda_report_item_label_get_visible (GdaReportItem *item);

gboolean gda_report_item_label_set_text (GdaReportItem *item,
					 const gchar *value);

gchar *gda_report_item_label_get_text (GdaReportItem *item);

gboolean gda_report_item_label_set_x (GdaReportItem *item,
				      GdaReportNumber *number);

GdaReportNumber *gda_report_item_label_get_x (GdaReportItem *item);

gboolean gda_report_item_label_set_y (GdaReportItem *item,
				      GdaReportNumber *number);

GdaReportNumber *gda_report_item_label_get_y (GdaReportItem *item);

gboolean gda_report_item_label_set_width (GdaReportItem *item,
					  GdaReportNumber *number);

GdaReportNumber *gda_report_item_label_get_width (GdaReportItem *item);

gboolean gda_report_item_label_set_height (GdaReportItem *item,
					   GdaReportNumber *number);

GdaReportNumber *gda_report_item_label_get_height (GdaReportItem *item);


gboolean gda_report_item_label_set_bgcolor (GdaReportItem *item,
	 			            GdaReportColor *value);

GdaReportColor *gda_report_item_label_get_bgcolor (GdaReportItem *item);

gboolean gda_report_item_label_set_fgcolor (GdaReportItem *item,
	 			            GdaReportColor *value);

GdaReportColor *gda_report_item_label_get_fgcolor (GdaReportItem *item);

gboolean gda_report_item_label_set_bordercolor (GdaReportItem *item,
					        GdaReportColor *value);

GdaReportColor *gda_report_item_label_get_bordercolor (GdaReportItem *item);

gboolean gda_report_item_label_set_borderwidth (GdaReportItem *item,
						GdaReportNumber *number);

GdaReportNumber *gda_report_item_label_get_borderwidth (GdaReportItem *item);

gboolean gda_report_item_label_set_borderstyle (GdaReportItem *item,
						const gchar *value);

gchar *gda_report_item_label_get_borderstyle (GdaReportItem *item);

gboolean gda_report_item_label_set_fontfamily (GdaReportItem *item,
					       const gchar *value);

gchar *gda_report_item_label_get_fontfamily (GdaReportItem *item);

gboolean gda_report_item_label_set_fontsize (GdaReportItem *item,
					     GdaReportNumber *number);

GdaReportNumber *gda_report_item_label_get_fontsize (GdaReportItem *item);

gboolean gda_report_item_label_set_fontweight (GdaReportItem *item,
					       const gchar *value);

gchar *gda_report_item_label_get_fontweight (GdaReportItem *item);

gboolean gda_report_item_label_set_fontitalic (GdaReportItem *item,
					       const gchar *value);

gchar *gda_report_item_label_get_fontitalic (GdaReportItem *item);

gboolean gda_report_item_label_set_halignment (GdaReportItem *item,
					       const gchar *value);

gchar *gda_report_item_label_get_halignment (GdaReportItem *item);

gboolean gda_report_item_label_set_valignment (GdaReportItem *item,
					       const gchar *value);

gchar *gda_report_item_label_get_valignment (GdaReportItem *item);

gboolean gda_report_item_label_set_wordwrap (GdaReportItem *item,
					     const gchar *value);

gchar *gda_report_item_label_get_wordwrap (GdaReportItem *item);


G_END_DECLS


#endif
