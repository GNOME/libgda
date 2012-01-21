/*
 * Copyright (C) 2009 - 2010 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "favorites-actions.h"
#include "../browser-connection.h"
#include <libgda/gda-sql-builder.h>
#include <sql-parser/gda-sql-parser.h>

static gint
actions_sort_func (ToolsFavoriteAction *act1, ToolsFavoriteAction *act2)
{
	return act2->nb_bound - act1->nb_bound;
}

/**
 * tools_favorites_get_actions
 * @bfav: a #ToolsFavorites
 * @bcnc: a #BrowserConnection
 * @set: a #GdaSet
 *
 * Get a list of #ToolsFavoriteAction which can be executed with the data in @set.
 *
 * Returns: a new list of #ToolsFavoriteAction, free list with tools_favorites_free_actions()
 */
GSList *
tools_favorites_get_actions (ToolsFavorites *bfav, BrowserConnection *bcnc, GdaSet *set)
{
	GSList *fav_list, *list, *retlist = NULL;
	g_return_val_if_fail (TOOLS_IS_FAVORITES (bfav), NULL);
	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), NULL);
	g_return_val_if_fail (!set || GDA_IS_SET (set), NULL);

	fav_list = tools_favorites_list (bfav, 0, TOOLS_FAVORITES_ACTIONS, -1, NULL);
	if (! fav_list)
		return NULL;

	for (list = fav_list; list; list = list->next) {
		ToolsFavoritesAttributes *fa = (ToolsFavoritesAttributes*) list->data;
		ToolsFavoritesAttributes qfa;
		if (! g_str_has_prefix (fa->contents, "QUERY")) {
			g_warning ("Malformed action contents '%s', please report error to "
				   "http://bugzilla.gnome.org/ for the \"libgda\" product",
				   fa->contents);
			continue;
		}
		if (tools_favorites_get (bfav, atoi (fa->contents + 5), &qfa, NULL)) {
			GdaSet *params;
			GSList *plist;
			GdaBatch *batch;
			GdaStatement *stmt = NULL;
			GdaSqlParser *parser;
			const gchar *remain;
			const GSList *stmt_list;
			gint nb_bound = 0;

			parser = browser_connection_create_parser (bcnc);
			batch = gda_sql_parser_parse_string_as_batch (parser, qfa.contents, &remain, NULL);
			g_object_unref (parser);
			if (!batch) {
				tools_favorites_reset_attributes (&qfa);
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
				tools_favorites_reset_attributes (&qfa);
				continue;
			}
			
			if (! gda_statement_get_parameters (stmt, &params, NULL) || !params) {
				g_object_unref (stmt);
				tools_favorites_reset_attributes (&qfa);
				continue;
			}
			browser_connection_define_ui_plugins_for_stmt (bcnc, stmt, params);
			
			for (plist = params->holders; plist; plist = plist->next) {
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
				ToolsFavoriteAction *act;
				act = g_new0 (ToolsFavoriteAction, 1);
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
			tools_favorites_reset_attributes (&qfa);
		}
	}
	tools_favorites_free_list (fav_list);

	return retlist;
}

/**
 * tools_favorites_free_action
 * @action: (allow-none): a #ToolsFavoriteAction, or %NULL
 *
 * Frees @action
 */
void
tools_favorites_free_action (ToolsFavoriteAction *action)
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
 * tools_favorites_free_actions_list
 * @actions_list: (allow-none): a list of #ToolsFavoriteAction, or %NULL
 *
 * Free a list of #ToolsFavoriteAction (frees the list and each #ToolsFavoriteAction)
 */
void
tools_favorites_free_actions_list (GSList *actions_list)
{
	GSList *list;
	if (!actions_list)
		return;

	for (list = actions_list; list; list = list->next)
		tools_favorites_free_action ((ToolsFavoriteAction*) list->data);
	g_slist_free (actions_list);
}
