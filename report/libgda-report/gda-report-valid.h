/* GDA report libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Santi Camps <scamps@users.sourceforge.net>
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

#if !defined(__gda_report_valid_h__)
#  define __gda_report_valid_h__

#include <glib-object.h>

G_BEGIN_DECLS


#define GDA_TYPE_REPORT_VALID            (gda_report_valid_get_type())
#define GDA_REPORT_VALID(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_VALID, GdaReportValid))
#define GDA_REPORT_VALID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_VALID, GdaReportValidClass))
#define GDA_IS_REPORT_VALID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_REPORT_VALID))
#define GDA_IS_REPORT_VALID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_VALID))

typedef struct _GdaReportValid GdaReportValid;
typedef struct _GdaReportValidClass GdaReportValidClass;
typedef struct _GdaReportValidPrivate GdaReportValidPrivate;

struct _GdaReportValid {
	GObject object;
	GdaReportValidPrivate *priv;
};

struct _GdaReportValidClass {
	GObjectClass parent_class;
};


GType gda_report_valid_get_type (void);

GdaReportValid *gda_report_valid_load (void);

GdaReportValid *gda_report_valid_new_from_dom (xmlDtdPtr dtd);

xmlDtdPtr gda_report_valid_to_dom (GdaReportValid *valid);

gboolean gda_report_valid_validate_document (GdaReportValid *valid, 
					     xmlDocPtr document);

gboolean gda_report_valid_validate_element (GdaReportValid *valid, 
					    xmlNodePtr element);

gboolean gda_report_valid_validate_attribute (GdaReportValid *valid, 
					      const gchar *element_name, 
					      const gchar *attribute_name, 
					      const gchar *value);

G_END_DECLS


#endif
