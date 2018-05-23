/* gda-ddl-base.h
 *
 * Copyright © 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
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

#ifndef GDA_DDL_BASE_H
#define GDA_DDL_BASE_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define GDA_TYPE_DDL_BASE (gda_ddl_base_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDdlBase, gda_ddl_base, GDA, DDL_BASE, GObject)

struct _GdaDdlBaseClass
{
	GObjectClass parent;
};

GdaDdlBase*		gda_ddl_base_new (void);

void			gda_ddl_base_set_names (GdaDdlBase *self,
										const gchar *catalog,
										const gchar *schema,
										const gchar *name);

const gchar*	gda_ddl_base_get_full_name	(GdaDdlBase *self);
const gchar*	gda_ddl_base_get_name		(GdaDdlBase *self);
void			gda_ddl_base_set_name		(GdaDdlBase *self,
											 const gchar *name);

const gchar*	gda_ddl_base_get_catalog	(GdaDdlBase *self);
void			gda_ddl_base_set_catalog	(GdaDdlBase  *self,
											 const gchar *catalog);

const gchar*	gda_ddl_base_get_schema		(GdaDdlBase *self);
void			gda_ddl_base_set_schema		(GdaDdlBase  *self,
											 const gchar *schema);

void			gda_ddl_base_free			(GdaDdlBase *self);
G_END_DECLS

#endif /* GDA_DDL_BASE_H */

