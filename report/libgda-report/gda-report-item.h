/* GDA report libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
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

#if !defined(__gda_report_item_h__)
#  define __gda_report_item_h__

#include <glib-object.h>
#include <libxml/tree.h>
#include <libgda-report/gda-report-valid.h>

G_BEGIN_DECLS

#define GDA_REPORT_TYPE_ITEM            (gda_report_item_get_type())
#define GDA_REPORT_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_REPORT_TYPE_ITEM, GdaReportItem))
#define GDA_REPORT_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_REPORT_TYPE_ITEM, GdaReportItemClass))
#define GDA_REPORT_IS_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_REPORT_TYPE_ITEM))
#define GDA_REPORT_IS_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_REPORT_TYPE_ITEM))

typedef struct _GdaReportItem GdaReportItem;
typedef struct _GdaReportItemClass GdaReportItemClass;
typedef struct _GdaReportItemPrivate GdaReportItemPrivate;

struct _GdaReportItem {
	GObject object;
	GdaReportItemPrivate *priv;
};

struct _GdaReportItemClass {
	GObjectClass parent_class;
};

/* Private structure in .h allowing use it in descendent classes */
struct _GdaReportItemPrivate {
	xmlNodePtr node;
	GdaReportValid *valid;
};


GType gda_report_item_get_type (void);


GdaReportItem *gda_report_item_new (GdaReportValid *valid,
				    const gchar *name);

/* GdaReportItem *gda_report_item_new_child (GdaReportItem *parent, 
				          const gchar *name);
*/

GdaReportItem *gda_report_item_new_from_dom (xmlNodePtr node);

gboolean gda_report_item_remove (GdaReportItem *item);

gboolean gda_report_item_add_previous (GdaReportItem *item,
				       GdaReportItem *new_item);

gboolean gda_report_item_add_next (GdaReportItem *item,
				   GdaReportItem *new_item);

gboolean gda_report_item_add_child (GdaReportItem *parent,
				    GdaReportItem *child);

gboolean gda_report_item_replace (GdaReportItem *item,
				  GdaReportItem *new_item);

xmlNodePtr gda_report_item_to_dom (GdaReportItem *item);

gchar *gda_report_item_get_item_type (GdaReportItem *item); 

gboolean gda_report_item_set_attribute (GdaReportItem *item,
	 			        const gchar *name,
				        const gchar *value);

gchar *gda_report_item_get_attribute (GdaReportItem *item, 
				      const gchar *name);

gchar *gda_report_item_get_inherit_attribute (GdaReportItem *item,
				 	      const gchar *name);
					      
GdaReportItem *gda_report_item_get_child_by_id (GdaReportItem *parent,
						const gchar *id);
						
gboolean gda_report_item_set_content (GdaReportItem *item,
	 			      const gchar *content);

gchar *gda_report_item_get_content (GdaReportItem *item);

gboolean gda_report_item_belongs_to_report_document (GdaReportItem *item);						

GdaReportItem *gda_report_item_get_report (GdaReportItem *item);

G_END_DECLS


#endif
