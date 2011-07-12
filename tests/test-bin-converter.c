/*
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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

int
main (int argc, char** argv)
{
	gchar *file;
	GdaBinary bin;
	gchar *bin_data;
	gsize bin_length;
	GError *error = NULL;

	/* load binary data */
	file = g_build_filename (CHECK_FILES, "data", "sales_test.db", NULL);
	if (! g_file_get_contents (file, &bin_data, &bin_length, &error)) {
		g_print ("Error reading binary file: %s\n", error->message);
		return EXIT_FAILURE;
	}		
	g_free (file);
	bin.data = (guchar*) bin_data;
	bin.binary_length = bin_length;

	/* convert to string */
	gchar *conv1;
	conv1 = gda_binary_to_string (&bin, 0);
	
	/* convert back to binary */
	GdaBinary *bin2;
	bin2 = gda_string_to_binary (conv1);
	
	/* compare bin */
	if (bin.binary_length != bin2->binary_length) {
		g_print ("Error: binary length differ: from %ld to %ld\n",
			 bin.binary_length, bin2->binary_length);
		return EXIT_FAILURE;
	}
	gint i;
	for (i = 0; i < bin.binary_length; i++) {
		if (bin.data[i] != bin2->data[i]) {
			g_print ("Error: binary differ orig[%d]=%d and copy[%d]=%d\n",
				 i, bin.data[i], i, bin2->data[i]);
			return EXIT_FAILURE;
		}
	}
	
	g_free (conv1);
	gda_binary_free (bin2);
	g_free (bin_data);

	g_print ("Ok (file size: %d)\n", bin_length);
	return EXIT_SUCCESS;
}
