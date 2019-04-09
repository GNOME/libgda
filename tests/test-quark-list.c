/*
 * Copyright (C) 2012 Vivien Malerba <malerba@gnome-db.org>
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

#include <libgda/libgda.h>
#include <string.h>

guint
test_quark (GdaQuarkList *ql, guint *out_ntests)
{
	const gchar *tmp;
	guint nfailed = 0;
	guint ntests = 0;

	ntests ++;
	tmp = gda_quark_list_find (ql, "PARAM");
	if (!tmp || strcmp (tmp, "value"))
		nfailed++;

	ntests ++;
	tmp = gda_quark_list_find (ql, "PASSWORD");
	if (!tmp || strcmp (tmp, "*mypass*"))
		nfailed++;

	ntests ++;
	tmp = gda_quark_list_find (ql, "PASSWORD");
	if (!tmp || strcmp (tmp, "*mypass*"))
		nfailed++;

	gda_quark_list_protect_values (ql);

	ntests ++;
	tmp = gda_quark_list_find (ql, "PASSWORD");
	if (!tmp || strcmp (tmp, "*mypass*"))
		nfailed++;

	ntests ++;
	tmp = gda_quark_list_find (ql, "USERNAME");
	if (!tmp || strcmp (tmp, "dirch"))
		nfailed++;

	gda_quark_list_remove (ql, "PASSWORD");

	ntests ++;
	tmp = gda_quark_list_find (ql, "PASSWORD");
	if (tmp)
		nfailed++;

	gda_quark_list_protect_values (ql);

	if (out_ntests)
		*out_ntests = ntests;
	return nfailed;
}

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
{
	GdaQuarkList *ql, *ql2;
	guint nfailed = 0;
	guint ntests = 0;
	guint tmp;

	ql = gda_quark_list_new_from_string ("PARAM=value;PASSWORD=*mypass*;USERNAME=dirch");
	ql2 = gda_quark_list_copy (ql);
	nfailed = test_quark (ql, &ntests);
	nfailed += test_quark (ql2, &tmp);
	ntests += tmp;

	/* out */
	gda_quark_list_free (ql);
	gda_quark_list_free (ql2);

	if (nfailed == 0) {
		g_print ("Ok (%d tests passed)\n", ntests);
		return EXIT_SUCCESS;
	}
	else {
		g_print ("Failed (%d tests failed out of %d)\n", nfailed, ntests);
		return EXIT_FAILURE;
	}
}
