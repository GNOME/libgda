/*
 * Copyright (C) 2006 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include <stdio.h>
#include <glib.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>

gint
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv) {
	GdaDataModel *model;

	gda_init ();
	g_setenv ("GDA_DATA_MODEL_DUMP_TITLE", "TRUE", TRUE);

	model = gda_config_list_providers ();
	gda_data_model_dump (model, stdout);
	g_object_unref (model);

	g_print ("\n\n");
	
	model = gda_config_list_dsn ();
	gda_data_model_dump (model, stdout);
	g_object_unref (model);

	return 0;
}
