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

#include "base-tool-command.h"
#include "base-tool-help.h"
#include "base-tool-errors.h"
#include <glib/gi18n-lib.h>

struct _ToolCommandGroup {
	GSList    *name_ordered;
        GSList    *group_ordered;
};

/**
 * base_tool_command_result_new:
 * @type: a #ToolCommandResultType
 *
 * Returns: (transfer full): a new #ToolCommandResult
 */
ToolCommandResult *
base_tool_command_result_new (GdaConnection *cnc, ToolCommandResultType type)
{
	ToolCommandResult *res;
	res = g_new0 (ToolCommandResult, 1);
	res->type = type;
	if (cnc) {
		res->was_in_transaction_before_exec =
			gda_connection_get_transaction_status (cnc) ? TRUE : FALSE;
		res->cnc = g_object_ref (cnc);
	}
	return res;
}

/**
 * base_tool_command_result_free:
 * @res: (nullable): a #ToolCommandResult, or %NULL
 *
 * Frees any resource used by @res
 */
void
base_tool_command_result_free (ToolCommandResult *res)
{
	if (!res)
		return;
	switch (res->type) {
	case BASE_TOOL_COMMAND_RESULT_DATA_MODEL:
		if (res->u.model)
			g_object_unref (res->u.model);
		if (! res->was_in_transaction_before_exec &&
		    res->cnc &&
		    gda_connection_get_transaction_status (res->cnc))
			gda_connection_rollback_transaction (res->cnc, NULL, NULL);
		break;
	case BASE_TOOL_COMMAND_RESULT_SET:
		if (res->u.set)
			g_object_unref (res->u.set);
		break;
	case BASE_TOOL_COMMAND_RESULT_TREE:
		if (res->u.tree)
			g_object_unref (res->u.tree);
		break;
	case BASE_TOOL_COMMAND_RESULT_TXT:
	case BASE_TOOL_COMMAND_RESULT_TXT_STDOUT:
		if (res->u.txt)
			g_string_free (res->u.txt, TRUE);
		break;
	case BASE_TOOL_COMMAND_RESULT_MULTIPLE: {
		GSList *list;

		for (list = res->u.multiple_results; list; list = list->next)
			base_tool_command_result_free ((ToolCommandResult *) list->data);
		g_slist_free (res->u.multiple_results);
		break;
	}
	case BASE_TOOL_COMMAND_RESULT_HELP:
		if (res->u.xml_node)
			xmlFreeNode (res->u.xml_node);
		break;
	case BASE_TOOL_COMMAND_RESULT_EXIT:
	case BASE_TOOL_COMMAND_RESULT_EMPTY:
		break;
	default:
		g_assert_not_reached ();
		break;
	}
	if (res->cnc)
		g_object_unref (res->cnc);
	g_free (res);
}

/**
 * base_tool_command_group_new:
 *
 * Returns: (transfer full): a new #ToolCommandGroup structure
 */
ToolCommandGroup *
base_tool_command_group_new (void)
{
	ToolCommandGroup *group;
	group = g_new0 (ToolCommandGroup, 1);
	return group;
}

/**
 * base_tool_command_group_free:
 * @group: (nullable) (transfer full): a #ToolCommandGroup pointer
 *
 * Frees any resource used by @group
 */
void
base_tool_command_group_free (ToolCommandGroup *group)
{
	if (group) {
		g_slist_free (group->name_ordered);
		g_slist_free (group->group_ordered);
		g_free (group);
	}
}

static gint
commands_compare_name (ToolCommand *a, ToolCommand *b)
{
	if (!a->name || !b->name) {
		g_warning (_("Invalid unnamed command"));
		if (!a->name) {
			if (b->name)
				return 1;
			else
				return 0;
		}
		else
			return -1;
	}
	return strcmp (a->name, b->name);
}

static gint
commands_compare_group (ToolCommand *a, ToolCommand *b)
{
	if (!a->group || !b->group) {
		g_warning (_("Invalid unnamed command"));
		if (!a->group) {
			if (b->group)
				return 1;
			else
				return 0;
		}
		else
			return -1;
	}
	return strcmp (a->group, b->group);
}

/**
 * base_tool_command_group_add:
 * @group: a #ToolCommandGroup pointer
 * @cmd: (transfer none): the command to add
 *
 * Add @cmd to @group. If a previous command with the same name existed,
 * it is replaced by the new one.
 *
 * @cmd is used as it is (i.e. not copied).
 */
void
base_tool_command_group_add (ToolCommandGroup *group, ToolCommand *cmd)
{
	g_return_if_fail (group);
	g_return_if_fail (cmd);
	g_return_if_fail (cmd->name && *cmd->name);
	g_return_if_fail (cmd->group && *cmd->group);
	if (cmd->name && !cmd->name_args) {
		cmd->name_args = cmd->name;
		gchar *tmp;
		for (tmp = cmd->name_args; *tmp && !g_ascii_isspace(*tmp); tmp++);
		cmd->name = g_strndup (cmd->name_args, tmp - cmd->name_args);
	}

	base_tool_command_group_remove (group, cmd->name);

	group->name_ordered = g_slist_insert_sorted (group->name_ordered, cmd,
						     (GCompareFunc) commands_compare_name);
	group->group_ordered = g_slist_insert_sorted (group->group_ordered, cmd,
						      (GCompareFunc) commands_compare_group);
}

/**
 * base_tool_command_group_remove:
 * @group: a #ToolCommandGroup pointer
 * @name: the name of the command to remove
 *
 * Remove @cmd from @group. If @cmd is not in @group, then nothing is done.
 */
void
base_tool_command_group_remove (ToolCommandGroup *group, const gchar *name)
{
	g_return_if_fail (group);

	GSList *list;
	for (list = group->name_ordered; list; list = list->next) {
		ToolCommand *ec;
		gint cmp;
		ec = (ToolCommand*) list->data;
		cmp = strcmp (name, ec->name);
		if (!cmp) {
			group->name_ordered = g_slist_remove (group->name_ordered, ec);
			group->group_ordered = g_slist_remove (group->group_ordered, ec);
			break;
		}
		else if (cmp > 0)
			break;
	}
}

/**
 * base_tool_command_group_find:
 */
ToolCommand *
base_tool_command_group_find (ToolCommandGroup *group, const gchar *name, GError **error)
{
	ToolCommand *cmd = NULL;
	GSList *list;
        gsize length;

	g_return_val_if_fail (group, NULL);
        if (!name)
                return NULL;

        for (list = group->name_ordered; list; list = list->next) {
		ToolCommand *command;
                command = (ToolCommand*) list->data;
		gint cmp;
		cmp = strcmp (command->name, name);
                if (!cmp) {
			cmd = command;
			break;
                }
		else if (cmp > 0)
			break;
        }

	if (! cmd) {
		/* see if a shortcut version leads to a command */
		length = strlen (name);
		guint nmatch = 0;
		for (list = group->name_ordered; list; list = list->next) {
			ToolCommand *command;
			command = (ToolCommand*) list->data;
			if (!strncmp (command->name, name, MIN (length, strlen (command->name)))) {
				nmatch ++;
				cmd = command;
			}
		}
		if (nmatch != 1)
			cmd = NULL;
	}

        if (!cmd &&
            ((*name == 'h') || (*name == 'H')))
                cmd = base_tool_command_group_find (group, "?", NULL);

	if (!cmd) {
		g_set_error (error, BASE_TOOL_ERROR,
			     BASE_TOOL_COMMAND_COMMAND_NOT_FOUND_ERROR,
			     _("Command '%s' not found"), name);
	}

	return cmd;
}

/*
 * Splits the arguments, separated by:
 *   - space characters (g_ascii_isspace())
 *   - or enclosed between double quotes
 *   - separators or double quotes precedeed by a \ are not considered as such
 *
 * Any backslashed char is converted to only the char in the end
 */
static gchar **
split_command_string (const gchar *cmde, guint *out_n_args, GError **error)
{
	GArray *args;
	gchar *ptr, *str;
	g_assert (cmde && *cmde);
	g_assert (out_n_args);

	/* split things up */
	*out_n_args = 0;
	str = g_strdup (cmde);
	args = g_array_new (TRUE, FALSE, sizeof (gchar*));
	for (ptr = str; *ptr; ptr++) {
		gchar *tmp;
		gboolean inquotes = FALSE;
		
		for (tmp = ptr; *tmp && g_ascii_isspace (*tmp); tmp++); /* ignore spaces */
		ptr = tmp;

		for (; *tmp; tmp++) {
			if (*tmp == '"')
				inquotes = !inquotes;
			else if (*tmp == '\\') {
				tmp++;
				if (! *tmp) {
					g_set_error (error, BASE_TOOL_ERROR,
						     BASE_TOOL_COMMAND_SYNTAX_ERROR,
						     _("Syntax error after '\\'"));
					goto onerror;
				}
			}
			else if (!inquotes && g_ascii_isspace (*tmp)) {
				gchar hold;
				gchar *dup;
				hold = *tmp;
				*tmp = 0;
				if (*ptr == '"') {
					gint len;
					dup = g_strdup (ptr + 1);
					len = strlen (dup);
					g_assert (dup [len-1] == '"');
					dup [len-1] = 0;
				}
				else
					dup = g_strdup (ptr);
				if (*dup) {
					g_array_append_val (args, dup);
					*out_n_args += 1;
				}
				else
					g_free (dup);
				ptr = tmp;
				*tmp = hold;
				break;
			}
		}

		if (!*tmp) {
			if (inquotes) {
				g_set_error (error, BASE_TOOL_ERROR,
					     BASE_TOOL_COMMAND_SYNTAX_ERROR,
					     _("Unbalanced usage of quotes"));
				goto onerror;
			}
			else {
				/* last command */
				gchar *dup;
				if (*ptr == '"') {
					gint len;
					dup = g_strdup (ptr + 1);
					len = strlen (dup);
					g_assert (dup [len-1] == '"');
					dup [len-1] = 0;
				}
				else
					dup = g_strdup (ptr);

				if (*dup) {
					g_array_append_val (args, dup);
					*out_n_args += 1;
				}
				else
					g_free (dup);
				break;
			}
		}
	}
	g_free (str);

	/* remove backslashes from each part */
	guint i;
	for (i = 0; ; i++) {
		gchar *tmp;
		tmp = g_array_index (args, gchar*, i);
		if (!tmp)
			break;
		gchar *tptr;
		for (ptr = tmp, tptr = tmp; *ptr; ptr++, tptr++) {
			if (*ptr == '\\')
				ptr++;
			*tptr = *ptr;
		}
		*tptr = *ptr;
	}

#ifdef DEBUG
	g_print ("split [%s] => %d parts\n", cmde, *out_n_args);
	for (i = 0; ; i++) {
		gchar *tmp;
		tmp = g_array_index (args, gchar*, i);
		if (!tmp)
			break;
		g_print ("\t[%s]\n", tmp);
	}
#endif
	return (gchar **) g_array_free (args, FALSE);

 onerror:
	g_free (str);
	g_array_free (args, TRUE);
	*out_n_args = 0;
	return NULL;
}

/**
 * base_tool_command_is_internal:
 * @group: a #ToolCommandGroup pointer
 * @cmde: any command (internal or SQL)
 *
 * Returns: %TRUE if @cmde is considered as an internal command (starts with '.' or '\\')
 */
gboolean
base_tool_command_is_internal (const gchar *cmde)
{
	g_return_val_if_fail (cmde, FALSE);
	if ((*cmde == '.') || (*cmde == '\\'))
		return TRUE;
	else
		return FALSE;
}

/**
 * base_tool_command_group_execute:
 * @group: a #ToolCommandGroup pointer
 * @cmde: an internal command as a string (name + arguments), without the '.' or '\\' beginning
 * @limit_width: pass to %TRUE if, in case the command is a HELP command, the width must be limited by a terminal's width
 * @color_term: pass to %TRUE if, in case the command is a HELP command, the output will be for a terminal color
 * @user_data: (nullable): a pointer, see the #ToolCommandFunc type
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Executes @cmde, which must be an internal command.
 *
 * Returns: (transfer full): a new #ToolCommandResult result
 */
ToolCommandResult *
base_tool_command_group_execute (ToolCommandGroup *group, const gchar *cmde,
				 gpointer user_data, GError **error)
{
	g_return_val_if_fail (group, NULL);

	if (!cmde || !*cmde) {
		ToolCommandResult *res;
		res = g_new0 (ToolCommandResult, 1);
		res->type = BASE_TOOL_COMMAND_RESULT_EMPTY;
		return res;
	}

	ToolCommand *cmd;
	gchar **args;
	guint nargs;
	args = split_command_string (cmde, &nargs, error);
	if (!args)
		return NULL;

	ToolCommandResult *res = NULL;
	cmd = base_tool_command_group_find (group, args[0], error);
	if (!cmd) {
		if (args[0] && ((*args[0] == 'h') || (*args[0] == '?'))) {
			/* help requested */
			g_clear_error (error);
			res = base_tool_help_get_command_help (group, args [1], error);
			if (!res)
				return NULL;
		}
		else {
			g_strfreev (args);
			return NULL;
		}
	}

	if (cmd && !res) {
		if (cmd->command_func)
			res = cmd->command_func (cmd, nargs - 1, (const gchar**) args + 1, user_data, error);
		else
			g_warning ("Tool command has no associated function to execute");
	}
	g_strfreev (args);
	return res;
}

/**
 * base_tool_command_get_all_commands:
 * @group: a #ToolCommandGroup group of commands
 *
 * Get a list of all the commands (sorted by group) in @group
 *
 * Returns: (transfer none): a list of all the #ToolCommand commands
 */
GSList *
base_tool_command_get_all_commands (ToolCommandGroup *group)
{
	g_return_val_if_fail (group, NULL);
	return group->group_ordered;
}

/**
 * base_tool_command_get_commands:
 * @group: a #ToolCommandGroup group of commands
 * @prefix: (nullable): a prefix
 *
 * Get a list of all the commands (sorted by group) in @group, starting by @prefix
 *
 * Returns: (transfer container): a list of all the #ToolCommand commands, free using g_slist_free()
 */
GSList *
base_tool_command_get_commands (ToolCommandGroup *group, const gchar *prefix)
{
	g_return_val_if_fail (group, NULL);
	if (!prefix || !*prefix)
		return g_slist_copy (group->group_ordered);

	GSList *list, *ret = NULL;
	gsize len;
	len = strlen (prefix);
	for (list = group->group_ordered; list; list = list->next) {
		ToolCommand *tc;
		tc = (ToolCommand*) list->data;
		if (!strncmp (tc->name, prefix, len))
			ret = g_slist_prepend (ret, tc);
	}

	return g_slist_reverse (ret);
}
