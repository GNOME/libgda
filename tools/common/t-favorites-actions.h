/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __FAVORITES_ACTIONS_H_
#define __FAVORITES_ACTIONS_H_

#include <libgda/libgda.h>
#include "t-connection.h"
#include "t-favorites.h"

G_BEGIN_DECLS

typedef struct {
	gint                  id;
	gchar                *name;
	GdaStatement         *stmt;
	GdaSet               *params;
	gint                  nb_bound; /* number of GdaHolders in @params which are bound 
					 * to another GdaHolder */
} TFavoritesAction;

GSList             *t_favorites_actions_get  (TFavorites *bfav, TConnection *tcnc, GdaSet *set);
void                t_favorites_action_free  (TFavoritesAction *action);
void                t_favorites_actions_list_free (GSList *actions_list);

G_END_DECLS

#endif
