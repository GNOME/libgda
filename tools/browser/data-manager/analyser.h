/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __ANALYSER_H__
#define __ANALYSER_H__

#include <libgda/libgda.h>
#include <gtk/gtk.h>
#include "data-source.h"
#include "data-source-manager.h"

G_BEGIN_DECLS

GSList *data_manager_add_proposals_to_menu (GtkWidget *menu,
					    DataSourceManager *mgr, DataSource *source,
					    GtkWidget *error_attach_widget);

G_END_DECLS

#endif
