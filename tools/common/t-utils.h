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

#ifndef __T_UTILS__
#define __T_UTILS__

#include <libgda/libgda.h>
#include "t-context.h"
#include <base/base-tool-decl.h>

const gchar *t_utils_fk_policy_to_string (GdaMetaForeignKeyPolicy policy);
gchar       *t_utils_compute_prompt (TContext *console, gboolean in_command, gboolean for_readline, ToolOutputFormat format);

gchar      **t_utils_split_text_into_single_commands (TContext *console, const gchar *commands, GError **error);
gboolean     t_utils_command_is_complete (TContext *console, const gchar *command);

gboolean     t_utils_check_shell_argument (const gchar *arg);

void         t_utils_term_compute_color_attribute (void);

#endif
