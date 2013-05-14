/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 2 -*- */
/*
 * libgdadata
 * Copyright (C) Daniel Espinosa Ortiz 2013 <esodan@gmail.com>
 *
 * libgda is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libgda is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* inclusion guard */
#ifndef __GDA_SQL_EXPRESSION_H__
#define __GDA_SQL_EXPRESSION_H__

#include <glib-object.h>
/*
 * Potentially, include other headers on which this header depends.
 */

/*
 * Type macros.
 */
#define GDA_TYPE_SQL_EXPRESSION                  (gda_sql_expression_get_type ())
#define GDA_SQL_EXPRESSION(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDA_TYPE_SQL_EXPRESSION, GdaSqlSelect))
#define GDA_IS_SQL_EXPRESSION(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDA_TYPE_SQL_EXPRESSION))
#define GDA_SQL_EXPRESSION_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GDA_TYPE_SQL_EXPRESSION, GdaSqlSelectClass))
#define GDA_IS_SQL_EXPRESSION_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_SQL_EXPRESSION))
#define GDA_SQL_EXPRESSION_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GDA_TYPE_SQL_EXPRESSION, GdaSqlSelectClass))

typedef struct _GdaSqlSelect        GdaSqlSelect;
typedef struct _GdaSqlSelectClass   GdaSqlSelectClass;
typedef struct _GdaSqlSelectPrivate GdaSqlSelectPrivate;

struct _GdaSqlSelect
{
  GObject parent_instance;

  /* instance members */
  GdaSqlSelectPrivate *priv;
};

struct _GdaSqlSelectClass
{
  GObjectClass parent_class;

  /* class members */
};

/* used by GDA_TYPE_SQL_EXPRESSION */
GType              gda_sql_select_get_type        (void);
GdaSqlSelect      *gda_sql_select_new             (void);

void               gda_sql_select_set_distinct       (GdaSqlSelect *select,
                                                    gboolean distinct);
const GValue      *gda_sql_select_get_distinct       (GdaSqlSelect *select);

#endif /* __GDA_SQL_EXPRESSION_H__ */
