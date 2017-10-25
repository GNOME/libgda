/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __RT_PARSER_H__
#define __RT_PARSER_H__

#include <string.h>
#include <libxml/tree.h>
#include <glib.h>
#include <gda-value.h>
#include "gda-report-engine.h"

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
	GdaBinary *binary;
};

RtNode *rt_parse_text (const gchar *text);
void    rt_free_node (RtNode *node);
void    rt_dump_tree (RtNode *tree);
gchar  *rt_dump_to_string (RtNode *tree);

void parse_rich_text_to_docbook (GdaReportEngine *eng, xmlNodePtr top, const gchar *text);
void parse_rich_text_to_html (GdaReportEngine *eng, xmlNodePtr top, const gchar *text);

G_END_DECLS

#endif
