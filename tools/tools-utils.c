/*
 * Copyright (C) 2011 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "tools-utils.h"
#include <glib/gi18n-lib.h>

/**
 * tools_utils_fk_policy_to_string
 * @policy: a #GdaMetaForeignKeyPolicy
 *
 * Returns: the human readable version of @policy
 */
const gchar *
tools_utils_fk_policy_to_string (GdaMetaForeignKeyPolicy policy)
{
	switch (policy) {
	default:
		g_assert_not_reached ();
	case GDA_META_FOREIGN_KEY_UNKNOWN:
		return _("Unknown");
	case GDA_META_FOREIGN_KEY_NONE:
		return _("not enforced");
	case GDA_META_FOREIGN_KEY_NO_ACTION:
		return _("stop with error");
	case GDA_META_FOREIGN_KEY_RESTRICT:
		return _("stop with error, not deferrable");
	case GDA_META_FOREIGN_KEY_CASCADE:
		return _("cascade changes");
	case GDA_META_FOREIGN_KEY_SET_NULL:
		return _("set to NULL");
	case GDA_META_FOREIGN_KEY_SET_DEFAULT:
		return _("set to default value");
	}
}

/* module error */
GQuark
tools_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("tools_error");
        return quark;
}
