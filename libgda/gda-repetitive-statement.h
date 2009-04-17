/* GDA library
 * 
 * Copyright (C) Daniel Espinosa Ortiz 2008 <esodan@gmail.com>
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

#ifndef _GDA_REPETITIVE_STATEMENT_H_
#define _GDA_REPETITIVE_STATEMENT_H_

#include <glib-object.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_REPETITIVE_STATEMENT             (gda_repetitive_statement_get_type ())
#define GDA_REPETITIVE_STATEMENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDA_TYPE_REPETITIVE_STATEMENT, GdaRepetitiveStatement))
#define GDA_REPETITIVE_STATEMENT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GDA_TYPE_REPETITIVE_STATEMENT, GdaRepetitiveStatementClass))
#define GDA_IS_REPETITIVE_STATEMENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDA_TYPE_REPETITIVE_STATEMENT))
#define GDA_IS_REPETITIVE_STATEMENT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPETITIVE_STATEMENT))
#define GDA_REPETITIVE_STATEMENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GDA_TYPE_REPETITIVE_STATEMENT, GdaRepetitiveStatementClass))

struct _GdaRepetitiveStatementClass
{
	GObjectClass parent_class;
};

struct _GdaRepetitiveStatement
{
	GObject parent_instance;
};

GType                   gda_repetitive_statement_get_type         (void) G_GNUC_CONST;

GdaRepetitiveStatement* gda_repetitive_statement_new              (GdaStatement *stmt);
gboolean                gda_repetitive_statement_get_template_set (GdaRepetitiveStatement *rstmt, GdaSet **set, GError **error);
GSList                 *gda_repetitive_statement_get_all_sets     (GdaRepetitiveStatement *rstmt);
gboolean                gda_repetitive_statement_append_set       (GdaRepetitiveStatement *rstmt, GdaSet *values, gboolean make_copy);

G_END_DECLS

#endif /* _GDA_REPETITIVE_STATEMENT_H_ */
