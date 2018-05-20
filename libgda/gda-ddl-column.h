/* gda-ddl-column.h
 *
 * Copyright © 2018 Pavlo Solntsev <pavlo.solntsev@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef GDA_DDL_COLUMN_H
#define GDA_DDL_COLUMN_H

#include <glib-object.h>
#include <glib.h>
#include <libxml/parser.h>

G_BEGIN_DECLS

#define GDA_TYPE_DDL_COLUMN (gda_ddl_column_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDdlColumn, gda_ddl_column, GDA, DDL_COLUMN, GObject)

     struct _GdaDdlColumnClass
{
  GObjectClass parent;
};

typedef  enum {
    GDA_DDL_COLUMN_TYPE,
    GDA_DDL_COLUMN_SET_NAME,
    GDA_DDL_COLUMN_SET_TYPE,
    GDA_DDL_COLUMN_SET_COMMENT,
    GDA_DDL_COLUMN_SET_DEFAULT,
    GDA_DDL_COLUMN_SET_AUTOINC,
    GDA_DDL_COLUMN_SET_NNUL,
    GDA_DDL_COLUMN_SET_UNIQUE,
    GDA_DDL_COLUMN_SET_SIZE,
    GDA_DDL_COLUMN_SET_PKEY,
    GDA_DDL_COLUMN_SET_CHECK
}GdaDdlColumnError;

#define GDA_DDL_COLUMN_ERROR gda_ddl_column_error_quark()
GQuark gda_ddl_column_error_quark (void);

GdaDdlColumn*   gda_ddl_column_new                      (void);

void            gda_ddl_column_free             (GdaDdlColumn *self);
gboolean        gda_ddl_column_parse_node       (GdaDdlColumn  *self,
                                                 xmlNodePtr     node,
                                                 GError **error); 
const gchar*    gda_ddl_column_get_name         (GdaDdlColumn *self); 
void            gda_ddl_column_set_name         (GdaDdlColumn *self, const gchar *name);

GType           gda_ddl_column_get_gtype        (GdaDdlColumn *self);
const gchar*    gda_ddl_column_get_ctype        (GdaDdlColumn *self); 
void            gda_ddl_column_set_type         (GdaDdlColumn *self, GType type);

guint                   gda_ddl_column_get_scale        (GdaDdlColumn *self);
void                    gda_ddl_column_set_scale        (GdaDdlColumn *self, guint scale);

gboolean                gda_ddl_column_get_pkey         (GdaDdlColumn *self);
void                    gda_ddl_column_set_pkey         (GdaDdlColumn *self, gboolean pkey);

gboolean                gda_ddl_column_get_nnul         (GdaDdlColumn *self);
void                    gda_ddl_column_set_nnul         (GdaDdlColumn *self, gboolean nnul);

gboolean                gda_ddl_column_get_autoinc      (GdaDdlColumn *self);
void                    gda_ddl_column_set_autoinc      (GdaDdlColumn *self, gboolean autoinc);

gboolean                gda_ddl_column_get_unique       (GdaDdlColumn *self);
void                    gda_ddl_column_set_unique       (GdaDdlColumn *self, gboolean unique);

const gchar*    gda_ddl_column_get_comment      (GdaDdlColumn *self);
void                    gda_ddl_column_set_comment      (GdaDdlColumn *self, const gchar *comnt);

guint                   gda_ddl_column_get_size         (GdaDdlColumn *self);
void                    gda_ddl_column_set_size         (GdaDdlColumn *self, guint size);

const gchar*    gda_ddl_column_get_default      (GdaDdlColumn *self);
void                    gda_ddl_column_set_default      (GdaDdlColumn *self, const gchar *value);

const gchar*    gda_ddl_column_get_check        (GdaDdlColumn *self);
void                    gda_ddl_column_set_check        (GdaDdlColumn *self, const gchar *value);


G_END_DECLS

#endif /* GDA_DDL_COLUMN_H */

