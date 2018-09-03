/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef _GDA_REPETITIVE_STATEMENT_H_
#define _GDA_REPETITIVE_STATEMENT_H_

#include <glib-object.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-set.h>

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

/**
 * SECTION:gda-repetitive-statement
 * @short_description: Execute the same statement several times with different values
 * @title: GdaRepetitiveStatement
 * @stability: Stable
 * @see_also: #GdaStatement, #GdaBatch and #GdaConnection
 *
 * The #GdaRepetitiveStatement object allows one to specify a statement to be executed
 * several times using different variables' values sets for each execution. Using the object
 * has almost no interrest at all if the statement to be executed several times has no parameter.
 *
 * Use the gda_connection_repetitive_statement_execute() method to execute the repetitive statement.
 */

GType                   gda_repetitive_statement_get_type         (void) G_GNUC_CONST;

GdaRepetitiveStatement* gda_repetitive_statement_new              (GdaStatement *stmt);
gboolean                gda_repetitive_statement_get_template_set (GdaRepetitiveStatement *rstmt, GdaSet **set, GError **error);
GSList                 *gda_repetitive_statement_get_all_sets     (GdaRepetitiveStatement *rstmt);
gboolean                gda_repetitive_statement_append_set       (GdaRepetitiveStatement *rstmt, GdaSet *values, gboolean make_copy);

G_END_DECLS

#endif
