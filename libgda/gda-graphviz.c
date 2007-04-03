/* gda-graphviz.c
 *
 * Copyright (C) 2003 - 2007 Vivien Malerba
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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "gda-graphviz.h"
#include "gda-query.h"
#include "gda-query-target.h"
#include "gda-query-join.h"
#include "gda-entity.h"
#include "gda-query-field.h"
#include "gda-entity-field.h"
#include "gda-query-field-all.h"
#include "gda-query-field-field.h"
#include "gda-query-field-value.h"
#include "gda-query-field-func.h"
#include "gda-xml-storage.h"
#include "gda-query-condition.h"
#include "gda-renderer.h"
#include "gda-referer.h"
#include "gda-parameter-list.h"
#include "gda-parameter.h"
#include "gda-parameter-util.h"

/* 
 * Main static functions 
 */
static void gda_graphviz_class_init (GdaGraphvizClass * class);
static void gda_graphviz_init (GdaGraphviz * srv);
static void gda_graphviz_dispose (GObject   * object);
static void gda_graphviz_finalize (GObject   * object);

#if 0 /* This object does not have any properties. */
static void gda_graphviz_set_property (GObject *object,
				    guint param_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void gda_graphviz_get_property (GObject *object,
				    guint param_id,
				    GValue *value,
				    GParamSpec *pspec);
#endif

static void weak_obj_notify (GdaGraphviz *graph, GObject *obj);
/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

#if 0 /* This object does not have any properties. */
/* properties */
enum
{
	PROP_0,
	PROP
};
#endif


/* private structure */
struct _GdaGraphvizPrivate
{
	GSList *graphed_objects;
};


/* module error */
GQuark gda_graphviz_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_graphviz_error");
	return quark;
}


GType
gda_graphviz_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaGraphvizClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_graphviz_class_init,
			NULL,
			NULL,
			sizeof (GdaGraphviz),
			0,
			(GInstanceInitFunc) gda_graphviz_init
		};

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaGraphviz", &info, 0);
	}
	return type;
}

static void
gda_graphviz_class_init (GdaGraphvizClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_graphviz_dispose;
	object_class->finalize = gda_graphviz_finalize;

        #if 0 /* This object does not have any properties. */
	/* Properties */
	object_class->set_property = gda_graphviz_set_property;
	object_class->get_property = gda_graphviz_get_property;

        /* TODO: What kind of object is this meant to be?
           When we know, we should use g_param_spec_object() instead of g_param_spec_pointer().
           murrayc.
         */
	g_object_class_install_property (object_class, PROP,
					 g_param_spec_pointer ("prop", NULL, NULL, 
							       (G_PARAM_READABLE | G_PARAM_WRITABLE)));
        #endif
}

static void
gda_graphviz_init (GdaGraphviz *gda_graphviz)
{
	gda_graphviz->priv = g_new0 (GdaGraphvizPrivate, 1);
	gda_graphviz->priv->graphed_objects = NULL;
}

/**
 * gda_graphviz_new
 * @dict: a #GdaDict object
 *
 * Creates a new #GdaGraphviz object
 *
 * Returns: the new object
 */
GObject*
gda_graphviz_new (GdaDict *dict)
{
	GObject *obj;
	GdaGraphviz *gda_graphviz;

	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);

	obj = g_object_new (GDA_TYPE_GRAPHVIZ, "dict", ASSERT_DICT (dict), NULL);
	gda_graphviz = GDA_GRAPHVIZ (obj);

	return obj;
}

static void
gda_graphviz_dispose (GObject *object)
{
	GdaGraphviz *gda_graphviz;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_GRAPHVIZ (object));

	gda_graphviz = GDA_GRAPHVIZ (object);
	if (gda_graphviz->priv) {
		while (gda_graphviz->priv->graphed_objects) {
			g_object_weak_unref (gda_graphviz->priv->graphed_objects->data, 
					     (GWeakNotify) weak_obj_notify, gda_graphviz);
			weak_obj_notify (gda_graphviz, gda_graphviz->priv->graphed_objects->data);
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gda_graphviz_finalize (GObject   * object)
{
	GdaGraphviz *gda_graphviz;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_GRAPHVIZ (object));

	gda_graphviz = GDA_GRAPHVIZ (object);
	if (gda_graphviz->priv) {
		g_free (gda_graphviz->priv);
		gda_graphviz->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

#if 0 /* This object does not have any properties. */
static void 
gda_graphviz_set_property (GObject *object,
			guint param_id,
			const GValue *value,
			GParamSpec *pspec)
{
	gpointer ptr;
	GdaGraphviz *gda_graphviz;

	gda_graphviz = GDA_GRAPHVIZ (object);
	if (gda_graphviz->priv) {
		switch (param_id) {
		case PROP:
			ptr = g_value_get_pointer (value);
			break;
		}
	}
}

static void
gda_graphviz_get_property (GObject *object,
			  guint param_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	GdaGraphviz *gda_graphviz;
	gda_graphviz = GDA_GRAPHVIZ (object);
	
	if (gda_graphviz->priv) {
		switch (param_id) {
		case PROP:
			break;
		}	
	}
}
#endif

/**
 * gda_graphviz_add_to_graph
 * @graph: a #GdaGraphviz object
 * @obj: a #GObject object to be graphed
 *
 * Adds @obj to be graphed by @graph
 */
void
gda_graphviz_add_to_graph (GdaGraphviz *graph, GObject *obj)
{
	g_return_if_fail (graph && GDA_IS_GRAPHVIZ (graph));
	g_return_if_fail (graph->priv);

	if (!g_slist_find (graph->priv->graphed_objects, obj)) {
		graph->priv->graphed_objects = g_slist_append (graph->priv->graphed_objects, obj);
		g_object_weak_ref (obj, (GWeakNotify) weak_obj_notify, graph);
	}
}

static void
weak_obj_notify (GdaGraphviz *graph, GObject *obj)
{
	graph->priv->graphed_objects = g_slist_remove (graph->priv->graphed_objects, obj);
}

/*
 * Graphing functions
 */
static GSList *prepare_queries (GdaGraphviz *graph);
static void    do_graph_query (GdaGraphviz *graph, GString *string, GdaQuery *query, gint taboffset);
static void    do_graph_context (GdaGraphviz *graph, GString *string, GdaParameterList *context, gint numcontext, gint taboffset);

/**
 * gda_graphviz_save_file
 * @graph: a #GdaGraphviz object
 * @filename:
 * @error:
 *
 * Saves a dot representation of the @graph object to @filename
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_graphviz_save_file (GdaGraphviz *graph, const gchar *filename, GError **error)
{
	gboolean allok = TRUE;
	GString *dot;
	FILE *file;
	size_t size;
	GSList *list, *tmplist;
	gint ncontext = 0;

	g_return_val_if_fail (graph && GDA_IS_GRAPHVIZ (graph), FALSE);
	g_return_val_if_fail (graph->priv, FALSE);

	/* File handling */
	file = fopen (filename, "w");
	if (!file) {
		TO_IMPLEMENT;
		return FALSE;
	}

	/* actual graph rendering */
	dot = g_string_new ("digraph G {\n"
			    "\tnode [shape=box];\n"
			    "\tnodesep = 0.5;\n");
	list = prepare_queries (graph);
	tmplist = list;
	while (list) {
		if (GDA_IS_QUERY (list->data)) 
			do_graph_query (graph, dot, GDA_QUERY (list->data), 1);
		list = g_slist_next (list);
	}
	g_slist_free (tmplist);

	list = graph->priv->graphed_objects;
	while (list) {
		if (GDA_IS_PARAMETER_LIST (list->data))
			do_graph_context (graph, dot, GDA_PARAMETER_LIST (list->data), ncontext++, 1);
		list = g_slist_next (list);
	}

	g_string_append (dot, "}\n");

	/* writing to filename */
	size = fwrite (dot->str, sizeof (gchar), strlen (dot->str), file);
	if (size != strlen (dot->str)) {
		TO_IMPLEMENT;
		allok = FALSE;
	}

	/* Finish */
	fclose (file);
	g_string_free (dot, TRUE);

	return allok;
}


/*
 * Queries
 */

static void    get_depend_query (GdaQuery *query, GSList **deplist, GSList **alldisplayed);

static void
prepare_single_query (GdaQuery *query, GSList **top_queries, GSList **all_queries)
{
	GSList *list;
	GSList *deplist, *alllist;

	get_depend_query (query, &deplist, &alllist);
	list = deplist;
	while (list) {
		if (!g_slist_find (*all_queries, list->data))
			prepare_single_query (GDA_QUERY (list->data), top_queries, all_queries);
		list = g_slist_next (list);
	}
	g_slist_free (deplist);

	if (alllist)
		*all_queries = g_slist_concat (*all_queries, alllist);

	if (!g_slist_find (*top_queries, query))
		*top_queries = g_slist_append (*top_queries, query);
	if (!g_slist_find (*all_queries, query))
		*all_queries = g_slist_append (*all_queries, query);
}

static void
get_depend_query (GdaQuery *query, GSList **deplist, GSList **alldisplayed)
{
	GSList *list, *tmplist;

	*deplist = NULL;
	*alldisplayed = NULL;

	list = gda_query_get_sub_queries (query);
	*alldisplayed = g_slist_concat (*alldisplayed, list);

	list = g_slist_copy ((GSList *) gda_query_get_param_sources (query));
	*alldisplayed = g_slist_concat (*alldisplayed, list);

	list = gda_query_get_all_fields (query);
	tmplist = list;
	while (list) {
		GdaObject *base;
		if (g_object_class_find_property (G_OBJECT_GET_CLASS (list->data), "value_provider")) {
			g_object_get (G_OBJECT (list->data), "value_provider", &base, NULL);
			if (base) {
				GdaQuery *dquery = GDA_QUERY (gda_entity_field_get_entity (GDA_ENTITY_FIELD (base)));
				if (!g_slist_find (*alldisplayed, dquery) && (dquery != query))
					*deplist = g_slist_append (*deplist, dquery);
				g_object_unref (base);
			}
		}
		list = g_slist_next (list);
	}
	g_slist_free (tmplist);
}

static GSList *
prepare_queries (GdaGraphviz *graph)
{
	GSList *top_queries = NULL, *all_queries = NULL, *list;

	list = graph->priv->graphed_objects;
	while (list) {
		if (GDA_IS_QUERY (list->data) && !g_slist_find (all_queries, list->data)) 
			prepare_single_query (GDA_QUERY (list->data), &top_queries, &all_queries);
		list = g_slist_next (list);
	}	

	g_slist_free (all_queries);

	return top_queries;
}

static gchar *render_qf_all_label (GdaGraphviz *graph, GdaQueryFieldAll *field);
static gchar *render_qf_field_label (GdaGraphviz *graph, GdaQueryFieldField *field);
static gchar *render_qf_value_label (GdaGraphviz *graph, GdaQueryFieldValue *field);
static gchar *render_qf_func_label (GdaGraphviz *graph, GdaQueryFieldFunc *field);
static void
do_graph_query (GdaGraphviz *graph, GString *string, GdaQuery *query, gint taboffset)
{
	gchar *str, *qid, *qtype, *sql;
	gint i;
	GSList *list;
	const GSList *clist;
	GdaQueryCondition *cond;

	/* offset string */
	str = g_new0 (gchar, taboffset+1);
	for (i=0; i<taboffset; i++)
		str [i] = '\t';
	
	qid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (query));
	g_string_append_printf (string, "%ssubgraph cluster%s {\n", str, qid);

	/* param sources */
	clist = gda_query_get_param_sources (query);
	if (clist) {
		g_string_append_printf (string, "%s\tsubgraph cluster%sps {\n", str, qid);
		g_string_append_printf (string, "%s\t\tnode [style=filled, color=black, fillcolor=white, fontsize=10];\n", str);
		while (clist) {
			do_graph_query (graph, string, GDA_QUERY (clist->data), taboffset+2);
			clist = g_slist_next (clist);
		}
		g_string_append_printf (string, "\n%s\t\tlabel = \"Parameter sources\";\n", str);
		g_string_append_printf (string, "%s\t\tstyle = filled;\n", str);
		g_string_append_printf (string, "%s\t\tcolor = white;\n", str);

		g_string_append_printf (string, "%s\t}\n", str);
	}
	
	/* sub queries */
	list = gda_query_get_sub_queries (query);
	if (list) {
		GSList *tmplist = list;
		g_string_append_printf (string, "%s\tsubgraph cluster%ssub {\n", str, qid);
		g_string_append_printf (string, "%s\t\tnode [style=filled, color=black, fillcolor=white, fontsize=10];\n", str);
		while (list) {
			do_graph_query (graph, string, GDA_QUERY (list->data), taboffset+2);
			list = g_slist_next (list);
		}
		g_string_append_printf (string, "\n%s\t\tlabel = \"Sub queries\";\n", str);
		g_string_append_printf (string, "%s\t\tstyle = filled;\n", str);
		g_string_append_printf (string, "%s\t\tcolor = white;\n", str);

		g_string_append_printf (string, "%s\t}\n", str);
		g_slist_free (tmplist);
	}
	

	/* targets */
	list = gda_query_get_targets (query);
	if (list) {
		GSList *tmplist = list;
		GdaEntity *ent;

		while (list) {
			gchar *tid;

			tid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
			g_string_append_printf (string, "%s\t\"%s\" ", str, tid);
			ent = gda_query_target_get_represented_entity (GDA_QUERY_TARGET (list->data));
			g_string_append_printf (string, "[label=\"%s (%s)\" style=filled fillcolor=orange];\n",
						gda_object_get_name (GDA_OBJECT (ent)), gda_query_target_get_alias (GDA_QUERY_TARGET (list->data)));
			g_free (tid);
			list = g_slist_next (list);
		}
		g_slist_free (tmplist);

		list = gda_query_get_joins (query);
		tmplist = list;
		while (list) {
			GdaQueryTarget *target;
			gchar *tid;
			gchar *jtype = "none";

			target = gda_query_join_get_target_1 (GDA_QUERY_JOIN (list->data));
			tid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (target));
			g_string_append_printf (string, "%s\t\"%s\" -> ", str, tid);
			g_free (tid);
			target = gda_query_join_get_target_2 (GDA_QUERY_JOIN (list->data));
			tid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (target));
			g_string_append_printf (string, "\"%s\" ", tid);
			g_free (tid);

			switch (gda_query_join_get_join_type (GDA_QUERY_JOIN (list->data))) {
			case GDA_QUERY_JOIN_TYPE_LEFT_OUTER:
				jtype = "back";
				break;
			case GDA_QUERY_JOIN_TYPE_RIGHT_OUTER:
				jtype = "forward";
				break;
			case GDA_QUERY_JOIN_TYPE_FULL_OUTER:
				jtype = "both";
				break;
			default:
				jtype = NULL;
			}
			if (jtype)
				g_string_append_printf (string, "[dir=%s, style=filled arrowhead=odot];\n", jtype);
			else
				g_string_append (string, "[dir=none, style=filled];\n");
			list = g_slist_next (list);
		}
		g_slist_free (tmplist);
	}

	/* fields */
	g_object_get (G_OBJECT (query), "really_all_fields", &list, NULL);
	if (list) {
		GSList *tmplist = list;

		while (list) {
			gchar *fid;
			gchar *label = NULL;

			if (GDA_IS_QUERY_FIELD_ALL (list->data))
				label = render_qf_all_label (graph, GDA_QUERY_FIELD_ALL (list->data));
			if (GDA_IS_QUERY_FIELD_FIELD (list->data))
				label = render_qf_field_label (graph, GDA_QUERY_FIELD_FIELD (list->data));
			if (GDA_IS_QUERY_FIELD_VALUE (list->data))
				label = render_qf_value_label (graph, GDA_QUERY_FIELD_VALUE (list->data));
			if (GDA_IS_QUERY_FIELD_FUNC (list->data))
				label = render_qf_func_label (graph, GDA_QUERY_FIELD_FUNC (list->data));
			if (!label)
				label = g_strdup ("{??? | {?|?|?|?}}");

			fid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
			g_string_append_printf (string, "%s\t\"%s\" ", str, fid);
			if (gda_referer_is_active (GDA_REFERER (list->data)))
				g_string_append_printf (string, "[shape=record, label=\"%s\", style=filled, fillcolor=deepskyblue1];\n",
							label);
			else
				g_string_append_printf (string, "[shape=record, label=\"%s\", style=filled, fillcolor=indianred];\n",
							label);
			g_free (label);
			g_free (fid);
			list = g_slist_next (list);
		}

		list = tmplist;
		while (list) {
			GdaQueryTarget *target = NULL;
			
			/* target link */
			if (GDA_IS_QUERY_FIELD_ALL (list->data))
				target = gda_query_field_all_get_target (GDA_QUERY_FIELD_ALL (list->data));
			if (GDA_IS_QUERY_FIELD_FIELD (list->data))
				target = gda_query_field_field_get_target (GDA_QUERY_FIELD_FIELD (list->data));

			if (target) {
				gchar *fid, *tid = NULL;
				fid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
				tid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (target));
				g_string_append_printf (string, "%s\t\"%s\" -> \"%s\";\n", str, fid, tid);
				g_free (fid);
				g_free (tid);
			}

			/* value provider link */
			if (g_object_class_find_property (G_OBJECT_GET_CLASS (list->data), "value_provider")) {
				GdaObject *base;

				g_object_get (G_OBJECT (list->data), "value_provider", &base, NULL);
				if (base) {
					gchar *fid, *bid;

					fid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
					bid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (base));
					g_string_append_printf (string, "%s\t\"%s\" -> \"%s\" [style=dotted];\n", str, fid, bid);
					g_free (fid);
					g_free (bid);
					g_object_unref (base);
				}
			}
			
			/* arguments for functions */
			if (GDA_IS_QUERY_FIELD_FUNC (list->data)) {
				GSList *args;

				args = gda_query_field_func_get_args (GDA_QUERY_FIELD_FUNC (list->data));
				if (args) {
					GSList *tmplist = args;
					gchar *fid;

					fid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
					while (args) {
						if (args->data) {
							gchar *did;

							did = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (args->data));
							g_string_append_printf (string, "%s\t\"%s\" -> \"%s\" [style=filled];\n", str, fid, did);
							g_free (did);
						}
						args = g_slist_next (args);
					}
					g_free (fid);
					g_slist_free (tmplist);
				}
			}

			list = g_slist_next (list);
		}
	}

	switch (gda_query_get_query_type (query)) {
	case GDA_QUERY_TYPE_SELECT:
		qtype = "SELECT";
		break;
	case GDA_QUERY_TYPE_INSERT:
		qtype = "INSERT";
		break;
	case GDA_QUERY_TYPE_UPDATE:
		qtype = "UPDATE";
		break;
	case GDA_QUERY_TYPE_DELETE:
		qtype = "DELETE";
		break;
	case GDA_QUERY_TYPE_UNION:
		qtype = "UNION";
		break;
	case GDA_QUERY_TYPE_INTERSECT:
		qtype = "INTERSECT";
		break;
	case GDA_QUERY_TYPE_EXCEPT:
		qtype = "EXCEPT";
		break;
	case GDA_QUERY_TYPE_NON_PARSED_SQL:
		qtype = "SQL";
		break;
	default:
		qtype = "???";
		break;
	}

	/* condition */
	cond = gda_query_get_condition (query);
	if (cond) {
		GSList *tmplist, *list;
		gchar *strcond;
		GError *error = NULL;

		strcond = gda_renderer_render_as_sql (GDA_RENDERER (cond), NULL, NULL, 0, &error);
		if (error) {
			g_string_append_printf (string, "%s\t\"%s:Cond\" [label=\"%s\" style=filled fillcolor=yellow];\n", str, qid,
						error->message);
			g_error_free (error);
		}
		else {
			g_string_append_printf (string, "%s\t\"%s:Cond\" [label=\"%s\" style=filled fillcolor=yellow];\n", str, qid, strcond);
			g_free (strcond);
		}

		list = gda_query_condition_get_ref_objects_all (cond);
		tmplist = list;
		while (list) {	
			gchar *fid;

			fid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (list->data));
			g_string_append_printf (string, "%s\t\"%s:Cond\" -> \"%s\";\n", str, qid, fid);
		
			list = g_slist_next (list);
		}
	}


	/* query attributes */
	sql = gda_renderer_render_as_sql (GDA_RENDERER (query), NULL, NULL, 0, NULL);
	g_string_append_printf (string, "\n%s\tlabel = \"%s Query %d: %s\";\n", str, qtype,
				gda_query_object_get_int_id (GDA_QUERY_OBJECT (query)), 
				gda_object_get_name (GDA_OBJECT (query)));
	g_free (sql);
	g_string_append_printf (string, "%s\tstyle = filled;\n", str);
	g_string_append_printf (string, "%s\tcolor = lightgrey;\n", str);
	g_string_append_printf (string, "%s}\n", str);
	g_free (qid);
	g_free (str);
}

static gchar *
render_qf_all_label (GdaGraphviz *graph, GdaQueryFieldAll *field)
{
	GString *retval;
	gchar *str;
	const gchar *cstr;

	retval = g_string_new ("{");
	cstr = gda_object_get_name (GDA_OBJECT (field));
	if (cstr)
		g_string_append (retval, cstr);

	g_string_append (retval, " | {*");

	if (gda_query_field_is_visible (GDA_QUERY_FIELD (field)))
		g_string_append (retval, " |V");
	else
		g_string_append (retval, " |-");
	if (gda_query_field_is_internal (GDA_QUERY_FIELD (field)))
		g_string_append (retval, " |I");
	else
		g_string_append (retval, " |-");
	g_string_append (retval, " |-}}");

	str = retval->str;
	g_string_free (retval, FALSE);
	return str;
}

static gchar *
render_qf_field_label (GdaGraphviz *graph, GdaQueryFieldField *field)
{
	GString *retval;
	gchar *str;
	const gchar *cstr;
	GdaEntityField *ref;

	retval = g_string_new ("{");
	cstr = gda_object_get_name (GDA_OBJECT (field));
	if (cstr)
		g_string_append (retval, cstr);
	ref = gda_query_field_field_get_ref_field (field);
	if (ref) {
		if (!cstr || strcmp (cstr, gda_object_get_name (GDA_OBJECT (ref))))
			g_string_append_printf (retval, " (%s)", gda_object_get_name (GDA_OBJECT (ref)));
	}

	g_string_append (retval, " | {Field");

	if (gda_query_field_is_visible (GDA_QUERY_FIELD (field)))
		g_string_append (retval, " |V");
	else
		g_string_append (retval, " |-");
	if (gda_query_field_is_internal (GDA_QUERY_FIELD (field)))
		g_string_append (retval, " |I");
	else
		g_string_append (retval, " |-");
	g_string_append (retval, " |-}}");

	str = retval->str;
	g_string_free (retval, FALSE);
	return str;
}

static gchar *
render_qf_value_label (GdaGraphviz *graph, GdaQueryFieldValue *field)
{
	GString *retval;
	gchar *str;
	const gchar *cstr;
	const GValue *value;

	retval = g_string_new ("{");
	cstr = gda_object_get_name (GDA_OBJECT (field));
	if (cstr)
		g_string_append (retval, cstr);
	value = gda_query_field_value_get_value (field);
	if (value) {
		str = gda_value_stringify ((GValue *) value);
		g_string_append_printf (retval, " (%s)", str);
		g_free (str);
	}

	g_string_append (retval, " | {Value");

	if (gda_query_field_is_visible (GDA_QUERY_FIELD (field)))
		g_string_append (retval, " |V");
	else
		g_string_append (retval, " |-");
	if (gda_query_field_is_internal (GDA_QUERY_FIELD (field)))
		g_string_append (retval, " |I");
	else
		g_string_append (retval, " |-");
	if (gda_query_field_value_get_is_parameter (field))
		g_string_append (retval, " |P}}");
	else
		g_string_append (retval, " |-}}");

	str = retval->str;
	g_string_free (retval, FALSE);
	return str;
}

static gchar *
render_qf_func_label (GdaGraphviz *graph, GdaQueryFieldFunc *field)
{
	GString *retval;
	gchar *str;
	const gchar *cstr;
	GdaDictFunction *func;

	retval = g_string_new ("{");
	cstr = gda_object_get_name (GDA_OBJECT (field));
	if (cstr)
		g_string_append (retval, cstr);
	func = gda_query_field_func_get_ref_func (field);
	if (func)
		g_string_append_printf (retval, " (%s)", gda_object_get_name (GDA_OBJECT (func)));

	g_string_append (retval, " | {Func()");

	if (gda_query_field_is_visible (GDA_QUERY_FIELD (field)))
		g_string_append (retval, " |V");
	else
		g_string_append (retval, " |-");
	if (gda_query_field_is_internal (GDA_QUERY_FIELD (field)))
		g_string_append (retval, " |I");
	else
		g_string_append (retval, " |-");
	g_string_append (retval, " |-}}");

	str = retval->str;
	g_string_free (retval, FALSE);
	return str;
}



/*
 * Context rendering
 */
static void
do_graph_context (GdaGraphviz *graph, GString *string, GdaParameterList *context, gint numcontext, gint taboffset)
{
	gchar *str;
	gint i, j;
	GSList *list;

	/* offset string */
	str = g_new0 (gchar, taboffset+1);
	for (i=0; i<taboffset; i++)
		str [i] = '\t';

	/* parameters */
	list = context->parameters;
	while (list) {
		GSList *dest;
		gchar *fid;

		g_string_append_printf (string, "%sParameter%p [label=\"%s (%d)\", shape=ellipse, style=filled, fillcolor=linen];\n", 
					str, list->data, gda_object_get_name (GDA_OBJECT (list->data)), numcontext);
		dest = gda_parameter_get_param_users (GDA_PARAMETER (list->data));
		while (dest) {
			fid = gda_xml_storage_get_xml_id (GDA_XML_STORAGE (dest->data));
			g_string_append_printf (string, "%sParameter%p -> \"%s\";\n", str, list->data, fid);
			g_free (fid);
			dest = g_slist_next (dest);
		}
		list = g_slist_next (list);
	}

	/* context nodes */
	j = 0;
	g_string_append_printf (string, "%ssubgraph clustercontext%d {\n", str, numcontext);	
	list = context->nodes_list;
	while (list) {
		GdaParameterListNode *node = GDA_PARAMETER_LIST_NODE (list->data);
		g_string_append_printf (string, "%s\tNode%p [label=\"Node%d\", shape=octagon];\n", str, list->data, j);
		
		g_string_append_printf (string, "%s\tNode%p -> Parameter%p [constraint=false];\n", 
					str, list->data, node->param);
		list = g_slist_next (list);
		j++;
	}
	g_string_append_printf (string, "%s\tlabel = \"Context %d\";\n", str, numcontext);
	g_string_append_printf (string, "%s}\n", str);
	

	g_free (str);
}
