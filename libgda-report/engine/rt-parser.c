/*
 * Copyright (C) 2011 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

#include "rt-parser.h"
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#ifdef HAVE_GDKPIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#endif

#include <libgda/gda-debug-macros.h>

/* RTE markup analysis */
typedef enum {
	MARKUP_NONE,      /* 0 */
	MARKUP_BOLD,
	MARKUP_TT,
	MARKUP_VERBATIM,
	MARKUP_ITALIC,
	MARKUP_STRIKE,    /* 5 */
	MARKUP_UNDERLINE,

	MARKUP_TITLE1_S,
	MARKUP_TITLE1_E,
	MARKUP_TITLE2_S,
	MARKUP_TITLE2_E,  /* 10 */

	MARKUP_PICTURE_S,
	MARKUP_PICTURE_E,

	MARKUP_LIST_S,
	MARKUP_LIST_E,

	MARKUP_EOF
} MarkupTag;

/* for the RtMarkup enum */
static gchar *markup_tag_text[] = {
	"NONE", "PARA", "BOLD", "TT", "VERBATIM", "ITALIC", "STRIKE", "UNDERLINE",
	"TITLE", "PICTURE", "LIST"
};

static
RtMarkup
internal_markup_to_external (MarkupTag markup, gint *out_offset)
{
	switch (markup) {
	case MARKUP_NONE:
		return RT_MARKUP_NONE;
	case MARKUP_BOLD:
		return RT_MARKUP_BOLD;
	case MARKUP_TT:
		return RT_MARKUP_TT;
	case MARKUP_VERBATIM:
		return RT_MARKUP_VERBATIM;
	case MARKUP_ITALIC:
		return RT_MARKUP_ITALIC;
	case MARKUP_STRIKE:
		return RT_MARKUP_STRIKE;
	case MARKUP_UNDERLINE:
		return RT_MARKUP_UNDERLINE;
	case MARKUP_TITLE1_S:
		*out_offset = 0;
		return RT_MARKUP_TITLE;
	case MARKUP_TITLE2_S:
		*out_offset = 1;
		return RT_MARKUP_TITLE;
	case MARKUP_PICTURE_S:
		return RT_MARKUP_PICTURE;
	case MARKUP_LIST_S:
		return RT_MARKUP_LIST;
	default:
		g_assert_not_reached ();
	}
	return MARKUP_NONE;
}

static MarkupTag get_markup_token (const gchar *alltext, const gchar *start, gint *out_nb_spaces_before,
				   const gchar **out_end, MarkupTag start_tag);
static MarkupTag get_token (const gchar *alltext, const gchar *start, gint *out_nb_spaces_before, const gchar **out_end,
			    MarkupTag start_tag);

/*
 * get_token
 *
 * returns the token type starting from @iter, and positions @out_end to the last used position
 * position.
 */
static MarkupTag
get_token (const gchar *alltext, const gchar *start, gint *out_nb_spaces_before, const gchar **out_end,
	   MarkupTag start_tag)
{
	MarkupTag retval;
	const gchar *ptr;

	retval = get_markup_token (alltext, start, out_nb_spaces_before, &ptr, start_tag);
	if ((retval != MARKUP_NONE) || (retval == MARKUP_EOF)) {
		*out_end = ptr;
		return retval;
	}

	for (; *ptr ; ptr = g_utf8_next_char (ptr)) {
		retval = get_markup_token (alltext, ptr, NULL, NULL, start_tag);
		if ((retval != MARKUP_NONE) || (retval == MARKUP_EOF))
			break;
	}
	*out_end = ptr;
	return MARKUP_NONE;
}

/*
 * get_markup_token
 * @alltext: the complete text
 * @start: starting point
 * @out_nb_spaces_before: a place to set the number of spaces since the start of line
 * @out_end: place to put the last used position, or %NULL
 * @start_tag: the starting tag, if any (to detect the closing tag)
 *
 * Parses marking tokens, nothing else
 *
 * Returns: a markup token, or MARKUP_NONE or MARKUP_EOF otherwise
 */
static MarkupTag
get_markup_token (const gchar *alltext, const gchar *start, gint *out_nb_spaces_before, const gchar **out_end,
		  MarkupTag start_tag)
{
	gchar c;
	gint ssol = -1; /* spaces since start of line */
	MarkupTag start_markup = start_tag;
	const gchar *ptr;

#define SET_OUT \
	if (out_end) {							\
		ptr++;							\
		*out_end = ptr;						\
	}								\
	if (out_nb_spaces_before)					\
		*out_nb_spaces_before = ssol

	if (start_tag)
		start_markup = start_markup;

	ptr = start;
	if (out_end)
		*out_end = ptr;
	if (out_nb_spaces_before)
		*out_nb_spaces_before = -1;
	c = *ptr;

	/* tests involving starting markup before anything else */
	if (start_markup == MARKUP_PICTURE_S) {
		if (c == ']') {
			ptr++;
			c = *ptr;
			if (c == ']') {
				ptr++;
				c = *ptr;
				if (c == ']') {
					SET_OUT;
					return MARKUP_PICTURE_E;
				}
			}
		}
		if (!c)
			return MARKUP_EOF;
		else
			return MARKUP_NONE;
	}
	else if (start_markup == MARKUP_VERBATIM) {
		if (c == '"') {
			ptr++;
			c = *ptr;
			if (c == '"') {
				ptr++;
				c = *ptr;
				if (c == '"') {
					SET_OUT;
					return MARKUP_VERBATIM;
				}
			}
		}
		if (!c)
			return MARKUP_EOF;
		else
			return MARKUP_NONE;
	}

	if ((*ptr == '\n') && (start_markup == MARKUP_LIST_S)) {
		SET_OUT;
		return MARKUP_LIST_E;
	}

	if (!c)
		return MARKUP_EOF;

	/* other tests */
	const gchar *ptr1 = ptr;
	if (ptr == alltext) {
		for (; *ptr1 == ' '; ptr1++);
		ssol = ptr1 - ptr;
	}
	else if (ptr[-1] == '\n') {
		for (; *ptr1 == ' '; ptr1++);
		ssol = ptr1 - ptr;
	}
	if (ssol >= 0) {
		/* we are on a line with only spaces since its start */
		if (ssol == 0) {
			if (c == '=') {
				ptr++;
				c = *ptr;
				if (c == ' ') {
					SET_OUT;
					return MARKUP_TITLE1_S;
				}
				else if (c == '=') {
					ptr++;
					c = *ptr;
					if (c == ' ') {
						SET_OUT;
						return MARKUP_TITLE2_S;
					}
				}
			}
		}
		
		c = *ptr1;
		if (c == '-') {
			ptr1++;
			c = *ptr1;
			if (c == ' ') {
				ptr = ptr1;
				SET_OUT;
				return MARKUP_LIST_S;
			}
		}
	}

	if (c == '*') {
		ptr++;
		c = *ptr;
		if (c == '*') {
			SET_OUT;
			return MARKUP_BOLD;
		}
	}
	else if (c == '/') {
		ptr++;
		c = *ptr;
		if (c == '/') {
			const gchar *bptr;
			bptr = ptr-2;
			if ((bptr > alltext) && (*bptr == ':')) {}
			else {
				SET_OUT;
				return MARKUP_ITALIC;
			}
		}
	}
	else if (c == '_') {
		ptr++;
		c = *ptr;
		if (c == '_') {
			SET_OUT;
			return MARKUP_UNDERLINE;
		}
	}
	else if (c == '-') {
		ptr++;
		c = *ptr;
		if (c == '-') {
			SET_OUT;
			return MARKUP_STRIKE;
		}
	}
	else if (c == '`') {
		ptr++;
		c = *ptr;
		if (c == '`') {
			SET_OUT;
			return MARKUP_TT;
		}
	}
	else if (c == '"') {
		ptr++;
		c = *ptr;
		if (c == '"') {
			ptr++;
			c = *ptr;
			if (c == '"') {
				SET_OUT;
				return MARKUP_VERBATIM;
			}
		}
	}
	else if (c == ' ') {
		ptr++;
		c = *ptr;
		if (c == '=') {
			if (start_markup == MARKUP_TITLE1_S) {
				/* ignore anything up to the EOL */
				for (; *ptr && (*ptr != '\n'); ptr++);

				SET_OUT;
				return MARKUP_TITLE1_E;
			}
			else {
				ptr++;
				c = *ptr;
				if (c == '=') {
					/* ignore anything up to the EOL */
					for (; *ptr && (*ptr != '\n'); ptr++);

					SET_OUT;
					return MARKUP_TITLE2_E;
				}
			}
		}
	}
	else if (c == '[') {
		ptr++;
		c = *ptr;
		if (c == '[') {
			ptr++;
			c = *ptr;
			if (c == '[') {
				SET_OUT;
				return MARKUP_PICTURE_S;
			}
		}
	}
	return MARKUP_NONE;
}

/*
 * steals @base64
 */
static gchar *
remove_newlines_from_base64 (gchar *base64)
{
	GString *string;
	gchar *ptr;
	string = g_string_new ("");
	for (ptr = base64; *ptr; ptr++) {
		if (*ptr != '\n')
			g_string_append_c (string, *ptr);
	}
	g_free (base64);
	return g_string_free (string, FALSE);
}

static gchar *
get_node_path (RtNode *node)
{
	gint i;
	RtNode *tmp;
	for (i = 0, tmp = node->prev; tmp; tmp = tmp->prev)
		i++;
	if (node->parent) {
		gchar *str, *ret;
		str = get_node_path (node->parent);
		ret = g_strdup_printf ("%s:%d", str, i);
		g_free (str);
		return ret;
	}
	else
		return g_strdup_printf ("%d", i);
}

static void
rt_dump_tree_offset (RtNode *tree, gint offset)
{
	gchar *str = "";

	if (offset) {
		str = g_new (gchar, offset + 1);
		memset (str, ' ', offset);
		str [offset] = 0;
	}

	g_print ("%p-%s%s ", tree, str, markup_tag_text[tree->markup]);

	gchar *path;
	path = get_node_path (tree);
	g_print ("[path=%s] ", path);
	g_free (path);

	if (tree->offset >= 0)
		g_print ("[offset=%d] ", tree->offset);

	if (tree->text) {
#define MAXSIZE 100
		g_print ("TEXT [");
		gchar *copy, *ptr;
		copy = g_strdup (tree->text);
		if (strlen (copy) > MAXSIZE) {
			copy [MAXSIZE] = 0;
			copy [MAXSIZE - 1] = '.';
			copy [MAXSIZE - 2] = '.';
			copy [MAXSIZE - 3] = '.';
		}
		for (ptr = copy; *ptr; ptr++) {
			if (*ptr == '\n') {
				g_print ("\n      %s", str);
			}
			else
				g_print ("%c", *ptr);
		}
		g_free (copy);
		g_print ("]\n");
	}
	else if (gda_binary_get_data (tree->binary)) {
		g_print ("BINARY\n");
	}
	else
		g_print ("\n");
	
	if (tree->child)
		rt_dump_tree_offset (tree->child, offset + 8);
	if (tree->next)
		rt_dump_tree_offset (tree->next, offset);
	if (offset)
		g_free (str);
}

void
rt_dump_tree (RtNode *tree)
{
	rt_dump_tree_offset (tree, 0);
}

static void
rt_dump_tree_to_string (RtNode *tree, GString *string)
{
	gchar *path;

	path = get_node_path (tree);

	g_string_append_printf (string, "%s-%s ", path, markup_tag_text[tree->markup]);
	g_free (path);

	if (tree->offset >= 0)
		g_string_append_printf (string, "[offset=%d] ", tree->offset);

	if (tree->text) {
		g_string_append (string, "TEXT [");
		gchar *ptr;
		for (ptr = tree->text; *ptr; ptr++) {
			if (*ptr == '\n') {
				g_string_append_c (string, '$');
			}
			else
				g_string_append_printf (string, "%c", *ptr);
		}
		g_string_append (string, "]|");
	}
	else if (gda_binary_get_data (tree->binary))
		g_string_append (string, "BINARY|");
	else
		g_string_append_c (string, '|');
	
	if (tree->child)
		rt_dump_tree_to_string (tree->child, string);
	if (tree->next)
		rt_dump_tree_to_string (tree->next, string);
}

gchar *
rt_dump_to_string (RtNode *tree)
{
	GString *string;
	string = g_string_new ("");
	rt_dump_tree_to_string (tree, string);
	return g_string_free (string, FALSE);
}

void
rt_free_node (RtNode *node)
{
	if (node->child)
		rt_free_node (node->child);
	if (node->next)
		rt_free_node (node->next);
	g_free (node->text);
	gda_binary_free (node->binary);
	g_free (node);
}

static gboolean merge_single_child_text (RtNode *tree);
static gboolean merge_text_node_child (RtNode *tree);
static gboolean merge_text_node_siblings (RtNode *tree);
static gboolean reorganize_children (RtNode *tree, gboolean *out_tree_destroyed);

/* if @tree is a RT_MARKUP_NONE with no binary data, and has a unique RT_MARKUP_NONE with no binary data child 
 * then merge the child into it */
static gboolean
merge_text_node_child (RtNode *tree)
{
	RtNode *child;
	child = tree->child;
	if (! child)
		return FALSE;

	if ((tree->markup == RT_MARKUP_NONE) && !gda_binary_get_data (tree->binary) &&
	    (child->markup == RT_MARKUP_NONE) &&  ! child->child && child->text && !gda_binary_get_data (child->binary)) {
		if (tree->text) {
			gchar *tmp;
			tmp = tree->text;
			tree->text = g_strconcat (tmp, child->text, NULL);
			g_free (tmp);
			g_free (child->text);
		}
		else
			tree->text = child->text;
		child->text = NULL;
		RtNode *tnode = child->next;
		child->next = NULL;
		rt_free_node (child);
		tree->child = tnode;
		return TRUE;
	}
	return FALSE;
}

/* if @tree is a RT_MARKUP_NONE with no binary data, then merge all the siblings which are also
 * RT_MARKUP_NONE into it */
static gboolean
merge_text_node_siblings (RtNode *tree)
{
	gboolean retval = FALSE;
	while (1) {
		if (! tree->next)
			break;
		RtNode *next = tree->next;
		if ((tree->markup == RT_MARKUP_NONE) && !gda_binary_get_data (tree->binary) &&
		    (next->markup == RT_MARKUP_NONE) && !gda_binary_get_data (next->binary) &&
		    ! next->child && next->text) {
			if (tree->text) {
				gchar *tmp;
				tmp = tree->text;
				tree->text = g_strconcat (tmp, next->text, NULL);
				g_free (tmp);
				g_free (next->text);
			}
			else
				tree->text = next->text;
			next->text = NULL;
			RtNode *tnode = next->next;
			next->next = NULL;
			rt_free_node (next);
			tree->next = tnode;

			retval = TRUE;
		}
		else
			break;
	}

	return retval;
}

static gboolean
merge_single_child_text (RtNode *tree)
{
	if (! (tree->text || gda_binary_get_data (tree->binary)) &&
	    tree->child && !tree->child->next &&
	    ! tree->child->child &&
	    (tree->child->text || gda_binary_get_data (tree->child->binary)) &&
	    (tree->child->markup == RT_MARKUP_NONE)) {
		tree->text = tree->child->text;
		tree->child->text = NULL;
		gda_binary_set_data (tree->binary, gda_binary_get_data (tree->child->binary), gda_binary_get_size (tree->child->binary));
		gda_binary_reset_data (tree->child->binary);
		rt_free_node (tree->child);
		tree->child = NULL;
		return TRUE;
	}

	return FALSE;
}

static gboolean
reorganize_children (RtNode *tree, gboolean *out_tree_destroyed)
{
	gboolean retval = FALSE;
	*out_tree_destroyed = FALSE;
	if ((tree->markup == RT_MARKUP_PARA) && (tree->text && !*(tree->text)) &&
	    !tree->child &&
	    tree->next && 
	    ((tree->next->markup == RT_MARKUP_LIST) || tree->next->markup == RT_MARKUP_TITLE)) {
		/* simply get rid of useless node */
		RtNode *n;
		n = tree->next;
		n->prev = tree->prev;
		if (tree->prev)
			tree->prev->next = n;
		if (tree->parent && (tree->parent->child == tree))
			tree->parent->child = n;
		tree->prev = NULL;
		tree->next = NULL;
		rt_free_node (tree);
		*out_tree_destroyed = TRUE;
		return TRUE;
	}
	else if ((tree->markup == RT_MARKUP_PARA) && (tree->text && !*(tree->text)) &&
		 !tree->child && !tree->next) {
		/* simply get rid of useless node */
		if (tree->prev)
			tree->prev->next = NULL;
		if (tree->parent && (tree->parent->child == tree))
			tree->parent->child = NULL;
		tree->prev = NULL;
		tree->next = NULL;
		rt_free_node (tree);
		*out_tree_destroyed = TRUE;
		return TRUE;
	}
	if (tree->markup == RT_MARKUP_LIST) {
		RtNode *node;
		for (node = tree->next; node;) {
			if ((node->markup != RT_MARKUP_LIST) ||
			    (node->offset <= tree->offset))
				break;
			RtNode *prev, *next;
			prev = node->prev;
			next = node->next;
			if (tree->child) {
				RtNode *n;
				for (n = tree->child; n->next; n = n->next);
				n->next = node;
				node->prev = n;
				node->next = NULL;
			}
			else {
				tree->child = node;
				node->prev = NULL;
				node->next = NULL;
			}
			if (prev)
				prev->next = next;
			if (next)
				next->prev = prev;
			node->parent = tree;
			node = next;
			retval = TRUE;
		}
	}
	else if (tree->markup == RT_MARKUP_TITLE) {
		RtNode *node;
		for (node = tree->next; node;) {
			if ((node->markup == RT_MARKUP_TITLE) &&
			    (node->offset <= tree->offset))
				break;

			RtNode *prev, *next;
			prev = node->prev;
			next = node->next;
			if (tree->child) {
				RtNode *n;
				for (n = tree->child; n->next; n = n->next);
				n->next = node;
				node->prev = n;
				node->next = NULL;
			}
			else {
				tree->child = node;
				node->prev = NULL;
				node->next = NULL;
			}
			if (prev)
				prev->next = next;
			if (next)
				next->prev = prev;
			node->parent = tree;
			node = next;
			retval = TRUE;
		}
	}
	else if (tree->markup == RT_MARKUP_PARA) {
		RtNode *node;
		for (node = tree->next; node;) {
			if ((node->markup == RT_MARKUP_TITLE) ||
			    (node->markup == RT_MARKUP_PARA))
				break;

			RtNode *prev, *next;
			prev = node->prev;
			next = node->next;
			if (tree->child) {
				RtNode *n;
				for (n = tree->child; n->next; n = n->next);
				n->next = node;
				node->prev = n;
				node->next = NULL;
			}
			else {
				tree->child = node;
				node->prev = NULL;
				node->next = NULL;
			}
			if (prev)
				prev->next = next;
			if (next)
				next->prev = prev;
			node->parent = tree;
			node = next;
			retval = TRUE;
		}
	}
	return retval;
}

/*
 * Simplifies and reorganizes the tree
 */
static gboolean
simplify_tree (RtNode *tree)
{
	gboolean mod, tree_del, retval = FALSE;

	for (mod = TRUE, tree_del = FALSE; mod && !tree_del;) {
		mod = FALSE;
		if (tree->child)
			mod = mod || simplify_tree (tree->child);
		if (tree->next)
			mod = mod || simplify_tree (tree->next);
		mod = mod || merge_single_child_text (tree);
		mod = mod || merge_text_node_child (tree);
		mod = mod || merge_text_node_siblings (tree);
		mod = mod || reorganize_children (tree, &tree_del);
		if (mod)
			retval = TRUE;
	}
	return retval;
}

static const gchar *
serialize_tag (MarkupTag tag)
{
	switch (tag) {
	case MARKUP_BOLD:
		return "**";
	case MARKUP_TT:
		return "``";
	case MARKUP_VERBATIM:
		return "\"\"";
	case MARKUP_ITALIC:
		return "//";
	case MARKUP_STRIKE:
		return "--";
	case MARKUP_UNDERLINE:
		return "__";
	case MARKUP_TITLE1_S:
		return "= ";
	case MARKUP_TITLE1_E:
		return " =";
	case MARKUP_TITLE2_S:
		return "== ";
	case MARKUP_TITLE2_E:
		return "=  ";
	case MARKUP_PICTURE_S:
		return "[[[";
	case MARKUP_PICTURE_E:
		return "]]]";
	case MARKUP_LIST_S:
		return "- ";
	default:
		g_assert_not_reached ();
	}
}

typedef struct {
	const gchar     *m_start;
	const gchar     *m_end;
	MarkupTag        markup;
	RtNode          *rtnode;
} TextTag;

static gboolean
markup_tag_match (TextTag *current, MarkupTag tag2, const gchar *last_position)
{
	const gchar *tmp;
	gboolean sameline = TRUE;
	for (tmp = current->m_start; *tmp && (tmp < last_position); tmp++) {
		if (*tmp == '\n') {
			sameline = FALSE;
			break;
		}
	}

	gboolean retval;
	switch (current->markup) {
	case MARKUP_BOLD:
	case MARKUP_TT:
	case MARKUP_VERBATIM:
	case MARKUP_ITALIC:
	case MARKUP_STRIKE:
	case MARKUP_UNDERLINE:
		retval = (current->markup == tag2) ? TRUE : FALSE;
		break;
	case MARKUP_TITLE1_S:
		retval = (tag2 == MARKUP_TITLE1_E) ? TRUE : FALSE;
		break;
	case MARKUP_TITLE2_S:
		retval = (tag2 == MARKUP_TITLE2_E) ? TRUE : FALSE;
		break;
	case MARKUP_PICTURE_S:
		retval = (tag2 == MARKUP_PICTURE_E) ? TRUE : FALSE;
		break;
	case MARKUP_LIST_S:
		retval = (tag2 == MARKUP_LIST_E) ? TRUE : FALSE;
		break;
	default:
		retval = FALSE;
		break;
	}

	if (retval) {
		if ((current->markup != MARKUP_PICTURE_S) && (current->markup != MARKUP_LIST_S) &&
		    (current->markup != MARKUP_VERBATIM))
			retval = sameline ? TRUE : FALSE;
	}
	return retval;
}

RtNode *
rt_parse_text (const gchar *text)
{
	RtNode *retnode, *contextnode;
	GList *queue = NULL; /* list of TextTag pointers */
	MarkupTag mt;
	const gchar *ptr, *prev;
	gint ssol;
	TextTag *current = NULL;

	retnode = g_new0 (RtNode, 1);
	retnode->binary = gda_binary_new ();
	contextnode = retnode;

	ptr = text;
	prev = text;

	for (mt = get_token (text, ptr, &ssol, &ptr, current ? current->markup : MARKUP_NONE);
	     mt != MARKUP_EOF;
	     mt = get_token (text, ptr, &ssol, &ptr, current ? current->markup : MARKUP_NONE)) {


#ifdef GDA_DEBUG_NO
		gchar *debug;
		debug = g_strndup (prev, ptr - prev);
		if (strlen (debug) > 10)
		       debug [10] = 0;
		g_print ("Token %d [%s] with SSOL %d\n", mt, debug, ssol);
		g_free (debug);
#endif

		if (mt == MARKUP_NONE) {
			gchar *part;
			RtNode *node;
			node = g_new0 (RtNode, 1);
			node->binary = gda_binary_new ();
			node->parent = contextnode;
			node->markup = RT_MARKUP_NONE;
			if (prev == text)
				node->markup = RT_MARKUP_PARA;
			else if (prev[-1] == '\n')
				node->markup = RT_MARKUP_PARA;
			part = g_strndup (prev, ptr - prev);

			if (contextnode->child) {
				RtNode *n;
				for (n = contextnode->child; n->next; n = n->next);
				n->next = node;
				node->prev = n;
			}
			else
				contextnode->child = node;

			if (contextnode->markup != RT_MARKUP_PICTURE) {
				/* split the node in multiple parts, one for each paragraph */
				gchar **array;
				gint i;
				array = g_strsplit (part, "\n", -1);
				for (i = 0; array [i]; i++) {
					if (! node->text)
						node->text = array [i];
					else {
						RtNode *n;
						n = g_new0 (RtNode, 1);
						n->binary = gda_binary_new ();
						n->parent = contextnode;
						n->markup = RT_MARKUP_PARA;
						n->text = array [i];
						node->next = n;
						n->prev = node;
						node = n;
					}
				}
				g_free (part);
			}
			else {
				gchar *tmp;
				tmp = remove_newlines_from_base64 (part);
				gsize size;
				guchar *buffer = g_base64_decode_inplace (tmp, & size);
				gda_binary_set_data (node->binary, buffer, size);
			}
		}
		else {
			gboolean tag_matched = FALSE;
			if (current) {
			retry:
				if (markup_tag_match (current, mt, ptr-1)) {
					/*g_print ("Tags matched for %d,%d\n",
					  current->markup, mt);*/
					g_free (current);
					queue = g_list_remove (queue, current);
					current = NULL;
					
					if (queue)
						current = (TextTag*) queue->data;
					tag_matched = TRUE;

					if (contextnode->parent)
						contextnode = contextnode->parent;
				}
				else {
					/* detect misplaced tags */
					GList *list;
					for (list = queue; list; list = list->next) {
						TextTag *tt = (TextTag*) list->data;
						
						if (markup_tag_match (tt, mt, ptr-1)) {
							/* remove all TextTag before @list */
							while (queue != list) {
								RtNode *lnode;
								current = (TextTag*) queue->data;
								lnode = current->rtnode;

								lnode->markup = RT_MARKUP_NONE;
								if (lnode->text) {
									gchar *tmp;
									tmp = lnode->text;
									lnode->text = g_strconcat (serialize_tag (current->markup),
												   tmp, NULL);
									g_free (tmp);
								}
								else if (gda_binary_get_data (lnode->binary)) {
									TO_IMPLEMENT;
								}
								else
								lnode->text = g_strdup (serialize_tag (current->markup));

								g_free (current);
								queue = g_list_delete_link (queue, queue);

							}
							g_assert (queue);
							current = (TextTag*) queue->data;
							contextnode = current->rtnode;

							goto retry;
						}
					}
				}
			}

			/*g_print ("Token %d with SSOL %d\n", mt, ssol);*/
			if (! tag_matched) {
				RtNode *node;
				node = g_new0 (RtNode, 1);
				node->binary = gda_binary_new ();
				node->parent = contextnode;
				node->offset = ssol;
				node->markup = internal_markup_to_external (mt, &(node->offset));
				
				if ((node->offset > 0) && (node->markup == RT_MARKUP_LIST)) {
					/* add missing list nodes if offset > 0 */
					if (contextnode->child) {
						RtNode *n;
						for (n = contextnode->child; n->next; n = n->next);

						gint i = 0;
                                                if (n->markup == RT_MARKUP_LIST)
                                                        i = n->offset + 1;
                                                for (; i < node->offset; i++) {
                                                        RtNode *tmpn;
                                                        tmpn = g_new0 (RtNode, 1);
                                                        tmpn->binary = gda_binary_new ();
                                                        tmpn->parent = contextnode;
                                                        tmpn->markup = RT_MARKUP_LIST;
                                                        tmpn->offset = i;
							tmpn->prev = n;
							n->next = tmpn;
							n = tmpn;
                                                }

						n->next = node;
						node->prev = n;
					}
					else {
						gint i;
						RtNode *n = NULL;
						for (i = 0; i < node->offset; i++) {
                                                        RtNode *tmpn;
                                                        tmpn = g_new0 (RtNode, 1);
                                                        tmpn->binary = gda_binary_new ();
                                                        tmpn->parent = contextnode;
                                                        tmpn->markup = RT_MARKUP_LIST;
                                                        tmpn->offset = i;
							if (n) {
								tmpn->prev = n;
								n->next = tmpn;
							}
							else
								contextnode->child = tmpn;
							n = tmpn;
                                                }
						g_assert (n);
						n->next = node;
						node->prev = n;
					}
				}
				else {
					if (contextnode->child) {
						RtNode *n;
						for (n = contextnode->child; n->next; n = n->next);
						n->next = node;
						node->prev = n;
					}
					else
						contextnode->child = node;
				}
				contextnode = node;				

				/* update @current */
				current = g_new0 (TextTag, 1);
				current->markup = mt;
				current->m_start = prev;
				current->m_end = ptr;
				current->rtnode = node;
				
				queue = g_list_prepend (queue, current);
			}
		}
		prev = ptr;
	}

	while (queue) {
		current = (TextTag*) queue->data;
		g_free (current);
		queue = g_list_delete_link (queue, queue);
	}

#ifdef GDA_DEBUG_NO
	g_print ("============= Before simplify_tree()\n");
	rt_dump_tree (retnode);
	simplify_tree (retnode);
	g_print ("============= After simplify_tree()\n");
	rt_dump_tree (retnode);
#else
	simplify_tree (retnode);
#endif
	return retnode;
}


/*
 *
 * Rendering
 *
 */

/*
 * @hash: key = rtnode, value = corresponding xmlNodePtr
 */
static gint file_nb = 0;
typedef struct {
	GHashTable *hash;
	gchar      *file_path;
	gchar      *file_prefix;
} RenderingContext;


/*
 * DocBook rendering
 */
static void
rich_text_node_to_docbook (RenderingContext *context, xmlNodePtr top_parent, RtNode *rtnode, xmlNodePtr parent)
{
	xmlNodePtr pattach = NULL, cattach = NULL;
	gchar *realtext;
	g_assert (parent);
	g_assert (context);

	if (rtnode->text) {
		gchar *optr, *nptr;
		gint len;
		len = strlen ((gchar*) rtnode->text);
		realtext = g_new (gchar, len + 1);
		for (optr = (gchar*) rtnode->text, nptr = realtext; *optr; optr++) {
			if (*optr != '\n') {
				*nptr = *optr;
				nptr++;
			}
		}
		*nptr = 0;
	}
	else
		realtext = (gchar *) rtnode->text;

	switch (rtnode->markup) {
	case RT_MARKUP_NONE:
		if (parent) {
			xmlNodeAddContent (parent, BAD_CAST realtext);
			cattach = parent;
		}
		else {
			cattach = xmlNewNode (NULL, BAD_CAST "para");
			xmlNodeAddContent (cattach, BAD_CAST realtext);
		}
		break;
	case RT_MARKUP_BOLD:
		cattach = xmlNewChild (parent, NULL, BAD_CAST "emphasis", BAD_CAST realtext);
		xmlSetProp (cattach, BAD_CAST "role", BAD_CAST "bold");
		break;
	case RT_MARKUP_PARA:
		pattach = parent;
		if ((parent != top_parent) &&
		     ! strcmp ((gchar*) parent->name, "para"))
			pattach = parent->parent;
		cattach = xmlNewChild (pattach, NULL, BAD_CAST "para", BAD_CAST realtext);
		parent = cattach;
		break;
	case RT_MARKUP_TT:
	case RT_MARKUP_VERBATIM:
	case RT_MARKUP_ITALIC:
		cattach = xmlNewChild (parent, NULL, BAD_CAST "emphasis", BAD_CAST realtext);
		break;
	case RT_MARKUP_STRIKE:
		cattach = xmlNewChild (parent, NULL, BAD_CAST "emphasis", BAD_CAST realtext);
		xmlSetProp (cattach, BAD_CAST "role", BAD_CAST "strikethrough");
		break;
	case RT_MARKUP_UNDERLINE:
		cattach = xmlNewChild (parent, NULL, BAD_CAST "emphasis", BAD_CAST realtext);
		xmlSetProp (cattach, BAD_CAST "role", BAD_CAST "underline");
		break;
	case RT_MARKUP_PICTURE: {
		gboolean saved = FALSE;
		gint type = 2; /* 0 for image, 1 for TXT and 2 for general binary */
		gchar *file, *tmp;
		tmp = g_strdup_printf ("%s_%04d.jpg", context->file_prefix,
				       file_nb ++);
		file = g_build_filename (context->file_path, tmp, NULL);
		g_free (tmp);

#ifdef HAVE_GDKPIXBUF
		GInputStream *istream;
		gpointer data;
		GError *error = NULL;
		data = gda_binary_get_data (rtnode->binary);
		if (data != NULL) {
			istream = g_memory_input_stream_new_from_data (data,
										   gda_binary_get_size (rtnode->binary),
										   NULL);
			GdkPixbuf *pixbuf = NULL;
			pixbuf = gdk_pixbuf_new_from_stream (istream, NULL, &error );
			if (pixbuf) {
				/* write to file */
				if (gdk_pixbuf_save (pixbuf, file, "jpeg", NULL,
						     "quality", "100", NULL)) {
					g_print ("Writen JPG file '%s'\n", file);
					saved = TRUE;
					type = 0;
				}
				g_object_unref (pixbuf);
			} else {
				g_message ("Error reading binary data pixbuf: %s",
						   error != NULL ? (error->message != NULL ? error->message : _("No detail"))
						   : _("No detail"));
			}
			g_object_unref (istream);
		}
#endif

		if (!saved) {
			if (gda_binary_get_data (rtnode->binary) &&
			    g_file_set_contents (file, (gchar*) gda_binary_get_data (rtnode->binary),
						 gda_binary_get_size (rtnode->binary), NULL)) {
				g_print ("Writen BIN file '%s'\n", file);
				saved = TRUE;
				type = 2;
			}
			else if (rtnode->text)
				type = 1;
		}
		if (! saved && (type != 1))
			TO_IMPLEMENT;
		else {
			switch (type) {
 			case 0:
				pattach =  xmlNewChild (parent, NULL, BAD_CAST "informalfigure",
							NULL);
				pattach =  xmlNewChild (pattach, NULL, BAD_CAST "mediaobject",
							NULL);
				pattach =  xmlNewChild (pattach, NULL, BAD_CAST "imageobject",
							NULL);
				cattach =  xmlNewChild (pattach, NULL, BAD_CAST "imagedata",
							NULL);
				xmlSetProp (cattach, BAD_CAST "fileref", BAD_CAST file);
				break;
 			case 1:
				xmlNodeAddContent (parent, BAD_CAST (rtnode->text));
				break;
 			case 2:
				cattach = xmlNewChild (parent, NULL, BAD_CAST "ulink",
						       BAD_CAST _("link"));
				xmlSetProp (cattach, BAD_CAST "url", BAD_CAST file);
				break;
			default:
				g_assert_not_reached ();
			}
		}
		g_free (file);
		break;
	}
	case RT_MARKUP_TITLE: {
		gchar *sect;
		pattach = parent;
		if (!strcmp ((gchar*) parent->name, "para"))
			pattach = parent->parent;
		sect = g_strdup_printf ("sect%d", rtnode->offset + 1);
		cattach = xmlNewChild (pattach, NULL, BAD_CAST sect, NULL);
		g_free (sect);
		pattach = xmlNewChild (cattach, NULL, BAD_CAST "title", BAD_CAST realtext);
		break;
	}
	case RT_MARKUP_LIST: {
		xmlNodePtr tmp = NULL;

		if (rtnode->prev &&
		    (rtnode->prev->markup == RT_MARKUP_LIST)) {
			tmp = g_hash_table_lookup (context->hash, rtnode->prev);
			g_assert (tmp);
			/* use the same <itemizedlist> */
			g_assert (!strcmp ((gchar*) tmp->name, "itemizedlist"));
			g_assert (rtnode->prev->offset == rtnode->offset);
			g_hash_table_insert (context->hash, rtnode, tmp);
			tmp = xmlNewChild (tmp, NULL, BAD_CAST "listitem", NULL);
			cattach = xmlNewChild (tmp, NULL, BAD_CAST "para", BAD_CAST realtext);
		}
		else {
			pattach = xmlNewChild (parent, NULL, BAD_CAST "itemizedlist", NULL);
			g_hash_table_insert (context->hash, rtnode, pattach);
			pattach = xmlNewChild (pattach, NULL, BAD_CAST "listitem", NULL);
			cattach = xmlNewChild (pattach, NULL, BAD_CAST "para", BAD_CAST realtext);
		}
		break;
	}
	default:
		if (rtnode->parent)
			g_assert_not_reached ();
		else
			cattach = parent;
		break;
	}

	if (rtnode->text)
		g_free (realtext);

	if (rtnode->child)
		rich_text_node_to_docbook (context, top_parent, rtnode->child, cattach);
	if (rtnode->next)
		rich_text_node_to_docbook (context, top_parent, rtnode->next, parent);
}

void
parse_rich_text_to_docbook (GdaReportEngine *eng, xmlNodePtr top, const gchar *text)
{
	RtNode *rtnode;
	RenderingContext context;
	g_return_if_fail (!eng || GDA_IS_REPORT_ENGINE (eng));

	context.hash = g_hash_table_new (NULL, NULL);
	context.file_path = ".";
	if (eng)
		g_object_get (eng, "output-directory", &context.file_path, NULL);
	context.file_prefix = "IMG";
	rtnode = rt_parse_text (text);
	/*rt_dump_tree (rtnode);*/
	rich_text_node_to_docbook (&context, top, rtnode, top);
	g_hash_table_destroy (context.hash);
	rt_free_node (rtnode);

	if (eng)
		g_free (context.file_path);
}

static xmlNodePtr
new_html_child (xmlNodePtr parent, const gchar *ns, const gchar *name, const gchar *contents)
{
	if (!parent || (parent->name && (*parent->name != 'h')))
		return xmlNewChild (parent, NULL, BAD_CAST name, BAD_CAST contents);
	else
		return new_html_child (parent->parent, ns, name, contents);
}

/*
 * HTML rendering
 */
static void
rich_text_node_to_html (RenderingContext *context, xmlNodePtr top_parent, RtNode *rtnode, xmlNodePtr parent)
{
	xmlNodePtr pattach = NULL, cattach = NULL;
	gchar *realtext;
	g_assert (parent);
	g_assert (context);

	if (rtnode->text) {
		gchar *optr, *nptr;
		gint len;
		len = strlen ((gchar*) rtnode->text);
		realtext = g_new (gchar, len + 1);
		for (optr = (gchar*) rtnode->text, nptr = realtext; *optr; optr++) {
			if (*optr != '\n') {
				*nptr = *optr;
				nptr++;
			}
		}
		*nptr = 0;
	}
	else
		realtext = (gchar *) rtnode->text;

	switch (rtnode->markup) {
	case RT_MARKUP_NONE:
		if (parent) {
			xmlNodeAddContent (parent, BAD_CAST realtext);
			cattach = parent;
		}
		else {
			cattach = xmlNewNode (NULL, BAD_CAST "para");
			xmlNodeAddContent (cattach, BAD_CAST realtext);
		}
		break;
	case RT_MARKUP_BOLD:
		cattach = new_html_child (parent, NULL, "b", realtext);
		break;
	case RT_MARKUP_PARA:
		pattach = parent;
		if ((parent != top_parent) &&
		     ! strcmp ((gchar*) parent->name, "p"))
			pattach = parent->parent;
		cattach = new_html_child (pattach, NULL, "p", realtext);
		parent = cattach;
		break;
	case RT_MARKUP_TT:
	case RT_MARKUP_VERBATIM:
	case RT_MARKUP_ITALIC:
		cattach = new_html_child (parent, NULL, "i", realtext);
		break;
	case RT_MARKUP_STRIKE:
		cattach = new_html_child (parent, NULL, "del", realtext);
		break;
	case RT_MARKUP_UNDERLINE:
		cattach = new_html_child (parent, NULL, "ins", realtext);
		break;
	case RT_MARKUP_PICTURE: {
		gboolean saved = FALSE;
		gint type = 2; /* 0 for image, 1 for TXT and 2 for general binary */
		gchar *file, *tmp;
		tmp = g_strdup_printf ("%s_%04d.jpg", context->file_prefix,
				       file_nb ++);
		file = g_build_filename (context->file_path, tmp, NULL);
		g_free (tmp);

#ifdef HAVE_GDKPIXBUF
		GInputStream *istream;
		gpointer data;
		GError *error = NULL;
		data = gda_binary_get_data (rtnode->binary);
		if (data != NULL) {
			istream = g_memory_input_stream_new_from_data (data,
										   gda_binary_get_size (rtnode->binary),
										   NULL);
			GdkPixbuf *pixbuf = NULL;
			pixbuf = gdk_pixbuf_new_from_stream (istream, NULL, &error );
			if (pixbuf) {
				/* write to file */
				if (gdk_pixbuf_save (pixbuf, file, "jpeg", NULL,
						     "quality", "100", NULL)) {
					g_print ("Writen JPG file '%s'\n", file);
					saved = TRUE;
					type = 0;
				}
				g_object_unref (pixbuf);
			} else {
				g_message ("Error reading binary data pixbuf: %s",
						   error != NULL ? (error->message != NULL ? error->message : _("No detail"))
						   : _("No detail"));
			}
			g_object_unref (istream);
		}
#endif

		if (!saved) {
			if (gda_binary_get_data (rtnode->binary) &&
			    g_file_set_contents (file, (gchar*) gda_binary_get_data (rtnode->binary),
						 gda_binary_get_size (rtnode->binary), NULL)) {
				g_print ("Writen BIN file '%s'\n", file);
				saved = TRUE;
				type = 2;
			}
			else if (rtnode->text)
				type = 1;
		}
		if (! saved && (type != 1))
			TO_IMPLEMENT;
		else {
			switch (type) {
 			case 0:
				pattach =  new_html_child (parent, NULL, "img",
							NULL);
				xmlSetProp (pattach, BAD_CAST "src", BAD_CAST file);
				break;
 			case 1:
				xmlNodeAddContent (parent, BAD_CAST (rtnode->text));
				break;
 			case 2:
				cattach = new_html_child (parent, NULL, "ulink",
						       _("link"));
				xmlSetProp (cattach, BAD_CAST "url", BAD_CAST file);
				break;
			default:
				g_assert_not_reached ();
			}
		}
		g_free (file);
		break;
	}
	case RT_MARKUP_TITLE: {
		gchar *sect;
		pattach = parent;
		if (!strcmp ((gchar*) parent->name, "para"))
			pattach = parent->parent;
		sect = g_strdup_printf ("h%d", rtnode->offset + 1);
		cattach = new_html_child (pattach, NULL, sect, realtext);
		g_free (sect);
		break;
	}
	case RT_MARKUP_LIST: {
		xmlNodePtr tmp = NULL;

		if (rtnode->prev &&
		    (rtnode->prev->markup == RT_MARKUP_LIST)) {
			tmp = g_hash_table_lookup (context->hash, rtnode->prev);
			g_assert (tmp);
			/* use the same <itemizedlist> */
			g_assert (!strcmp ((gchar*) tmp->name, "ul"));
			g_assert (rtnode->prev->offset == rtnode->offset);
			g_hash_table_insert (context->hash, rtnode, tmp);
			tmp = new_html_child (tmp, NULL, "li", NULL);
			cattach = new_html_child (tmp, NULL, "p", realtext);
		}
		else {
			pattach = new_html_child (parent, NULL, "ul", NULL);
			g_hash_table_insert (context->hash, rtnode, pattach);
			pattach = new_html_child (pattach, NULL, "li", NULL);
			cattach = new_html_child (pattach, NULL, "p", realtext);
		}
		break;
	}
	default:
		if (rtnode->parent)
			g_assert_not_reached ();
		else
			cattach = parent;
		break;
	}

	if (rtnode->text)
		g_free (realtext);

	if (rtnode->child)
		rich_text_node_to_html (context, top_parent, rtnode->child, cattach);
	if (rtnode->next)
		rich_text_node_to_html (context, top_parent, rtnode->next, parent);
}

void
parse_rich_text_to_html (GdaReportEngine *eng, xmlNodePtr top, const gchar *text)
{
	RtNode *rtnode;
	RenderingContext context;
	g_return_if_fail (!eng || GDA_IS_REPORT_ENGINE (eng));

	context.hash = g_hash_table_new (NULL, NULL);
	context.file_path = ".";
	if (eng)
		g_object_get (eng, "output-directory", &context.file_path, NULL);
	context.file_prefix = "IMG";
	rtnode = rt_parse_text (text);
	/*rt_dump_tree (rtnode);*/
	rich_text_node_to_html (&context, top, rtnode, top);
	g_hash_table_destroy (context.hash);
	rt_free_node (rtnode);

	if (eng)
		g_free (context.file_path);
}
