/* GdaXql: the xml query object
 * Copyright (C) 2000 Gerhard Dieringer
 * deGOBified by Cleber Rodrigues (cleber@gnome-db.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gda-xql-atom.h"

static void gda_xql_atom_init (GdaXqlAtom *xqlatom);
static void gda_xql_atom_class_init (GdaXqlAtomClass *klass);

static GdaXqlItemClass *parent_class = NULL;

GType
gda_xql_atom_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (GdaXqlAtomClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xql_atom_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GdaXqlAtom),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gda_xql_atom_init,
		};

		type = g_type_register_static (GDA_TYPE_XQL_ITEM, "GdaXqlAtom", &info, 0);
	}

	return type;
}

static void 
gda_xql_atom_init (GdaXqlAtom * xqlatom)
{

}

static void 
gda_xql_atom_class_init (GdaXqlAtomClass *klass)
{
	parent_class = g_type_class_peek_parent (klass);
}

GdaXqlItem * 
gda_xql_atom_new (gchar *tag)
{
        GdaXqlItem *atom;

	atom = g_object_new (GDA_TYPE_XQL_ATOM, NULL);

        gda_xql_item_set_tag (atom, tag);

        return atom;
}
