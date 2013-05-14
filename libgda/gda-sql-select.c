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

#include <gda-sql-select.h>
#include <sql-parser/gda-statement-struct-parts.h>

G_DEFINE_TYPE (GdaSqlSelect, gda_sql_select, G_TYPE_OBJECT);

#define GDA_SQL_SELECT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GDA_TYPE_SQL_SELECT, GdaSqlSelectPrivate))

struct _GdaSqlSelectPrivate
{
  GdaSqlStatementSelect  *sel;
};

/* properties */
enum
{
        PROP_0
};

static void
gda_sql_select_class_init (GdaSqlSelectClass *klass)
{
  g_type_class_add_private (klass, sizeof (GdaSqlSelectPrivate));
}

static void
gda_sql_select_init (GdaSqlSelect *self)
{
  GdaSqlSelectPrivate *priv;

  self->priv = priv = GDA_SQL_SELECT_GET_PRIVATE (self);

  priv->sel = g_new0 (GdaSqlStatementSelect, 1);
}
