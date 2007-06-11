/* GDA common library
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDA_BINRELOC_H__
#define __GDA_BINRELOC_H__

#include <glib.h>

G_BEGIN_DECLS

/*
 * Locating files
 */
typedef enum {
	GDA_NO_DIR,
	GDA_BIN_DIR,
	GDA_SBIN_DIR,
	GDA_DATA_DIR,
	GDA_LOCALE_DIR,
	GDA_LIB_DIR,
	GDA_LIBEXEC_DIR,
	GDA_ETC_DIR
} GdaPrefixDir;

void     gda_gbr_init          (void);
gchar   *gda_gbr_get_file_path (GdaPrefixDir where, ...);

G_END_DECLS

#endif
