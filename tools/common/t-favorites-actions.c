/*
 * Copyright (C) 2012 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "t-favorites-actions.h"
#include "t-connection.h"
#include <libgda/gda-sql-builder.h>
#include <sql-parser/gda-sql-parser.h>

static gint
actions_sort_func (TFavoritesAction *act1, TFavoritesAction *act2)
{
	return act2->nb_bound - act1->nb_bound;
}

/**
 * t_favorites_actions_get
 * @bfav: a #TFavorites
 * @tcnc: a #TConnection
 * @set: a #GdaSet
 *
 * Get a list of #TFavoritesAction which can be executed with the data in @set.
 *
 * Returns: a new list of #TFavoritesAction, free list with t_favorites_action_frees()
 */
GSList *
t_favorites_actions_get (TFavorites *bfav, TConnection *tcnc, GdaSet *set)
{
	GSList *fav_list, *list, *retlist = NULL;
	g_return_val_if_fail (T_IS_FAVORITES (bfav), NULL);
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (!set || GDA_IS_SET (set), NULL);

	fav_list = t_favorites_list (bfav, 0, T_FAVORITES_ACTIONS, -1, NULL);
	if (! fav_list)
		return NULL;

	for (list = fav_list; list; list = list->next) {
		TFavoritesAttributes *fa = (TFavoritesAttributes*) list->data;
		TFavoritesAttributes qfa;
		if (! g_str_has_prefix (fa->contents, "QUERY")) {
			g_warning ("Malformed action contents '%s', please report error to "
				   "http://gitlab.gnome.org/GNOME/libgda/issues",
				   fa->contents);
			continue;
		}
		if (t_favorites_get (bfav, atoi (fa->contents + 5), &qfa, NULL)) {
			GdaSet *params;
			GSList *plist;
			GdaBatch *batch;
			GdaStatement *stmt = NULL;
			GdaSqlParser *parser;
			const gchar *remain;
			const GSList *stmt_list;
			gint nb_bound = 0;

			parser = t_connection_create_parser (tcnc);
			batch = gda_sql_parser_parse_string_as_batch (parser, qfa.contents, &remain, NULL);
			g_object_unref (parser);
			if (!batch) {
				t_favorites_reset_attributes (&qfa);
				continue;
			}
			stmt_list = gda_batch_get_statements (batch);
			for (plist = (GSList*) stmt_list; plist; plist = plist->next) {
				if (! gda_statement_is_useless (GDA_STATEMENT (plist->data))) {
					if (stmt)
						break;
					else
						stmt = g_object_ref (GDA_STATEMENT (plist->data));
				}
			}
			g_object_unref (batch);
			if (!stmt || plist) {
				t_favorites_reset_attributes (&qfa);
				continue;
			}
			
			if (! gda_statement_get_parameters (stmt, &params, NULL) || !params) {
				g_object_unref (stmt);
				t_favorites_reset_attributes (&qfa);
				continue;
			}
			t_connection_define_ui_plugins_for_stmt (tcnc, stmt, params);
			
			for (plist = gda_set_get_holders (params); plist; plist = plist->next) {
				/* try to find holder in @set */
				GdaHolder *req_holder, *in_holder;
				req_holder = GDA_HOLDER (plist->data);
				in_holder = gda_set_get_holder (set, gda_holder_get_id (req_holder));
				if (in_holder && gda_holder_set_bind (req_holder, in_holder, NULL)) {
					/* bound this holder to the oune found */
					nb_bound++;
				}
			}

			if (nb_bound > 0) {
				/* at least 1 holder was found=> keep the action */
				TFavoritesAction *act;
				act = g_new0 (TFavoritesAction, 1);
				retlist = g_slist_insert_sorted (retlist, act,
								 (GCompareFunc) actions_sort_func);
				act->params = g_object_ref (params);
				act->id = fa->id;
				act->name = g_strdup (fa->name);
				act->stmt = g_object_ref (stmt);
				act->nb_bound = nb_bound;

				/*g_print ("Action identified: ID=%d Bound=%d name=[%s] SQL=[%s]\n",
				  act->id, act->nb_bound,
				  act->name, qfa.contents);*/
			}

			g_object_unref (stmt);
			g_object_unref (params);
			t_favorites_reset_attributes (&qfa);
		}
	}
	t_favorites_free_list (fav_list);

	return retlist;
}

/**
 * t_favorites_action_free
 * @action: (nullable): a #TFavoritesAction, or %NULL
 *
 * Frees @action
 */
void
t_favorites_action_free (TFavoritesAction *action)
{
	if (! action)
		return;
	g_free (action->name);
	if (action->stmt)
		g_object_unref (action->stmt);
	if (action->params)
		g_object_unref (action->params);
	g_free (action);
}

/**
 * t_favorites_actions_list_free
 * @actions_list: (nullable): a list of #TFavoritesAction, or %NULL
 *
 * Free a list of #TFavoritesAction (frees the list and each #TFavoritesAction)
 */
void
t_favorites_actions_list_free (GSList *actions_list)
{
	GSList *list;
	if (!actions_list)
		return;

	for (list = actions_list; list; list = list->next)
		t_favorites_action_free ((TFavoritesAction*) list->data);
	g_slist_free (actions_list);
}
