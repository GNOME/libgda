/*
 * Copyright (C) 2010 The GNOME Foundation.
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

#ifndef __RT_PARSER_H__
#define __RT_PARSER_H__

#include <string.h>
#include <libxml/tree.h>
#include <glib.h>
#include <gda-value.h>

G_BEGIN_DECLS

typedef enum {
	RT_MARKUP_NONE,
	RT_MARKUP_PARA,
	RT_MARKUP_BOLD,
	RT_MARKUP_TT,
	RT_MARKUP_VERBATIM,
	RT_MARKUP_ITALIC,
	RT_MARKUP_STRIKE,
	RT_MARKUP_UNDERLINE,
	RT_MARKUP_TITLE,
	RT_MARKUP_PICTURE,
	RT_MARKUP_LIST,
} RtMarkup;

typedef struct _RtNode RtNode;
struct _RtNode {
	RtNode    *parent;
	RtNode    *child;
	RtNode    *prev;
	RtNode    *next;

	RtMarkup   markup;
	gint       offset;
	gchar     *text;
	GdaBinary  binary;
};

RtNode *rt_parse_text (const gchar *text);
void    rt_free_node (RtNode *node);
void    rt_dump_tree (RtNode *tree);
gchar  *rt_dump_to_string (RtNode *tree);

void parse_rich_text_to_docbook (xmlNodePtr top, const gchar *text);

G_END_DECLS

#endif
