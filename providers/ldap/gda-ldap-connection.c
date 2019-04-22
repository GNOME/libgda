/*
 * Copyright (C) 2011 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gda-util.h>
#include "gda-ldap-connection.h"
#include <libgda/gda-connection-private.h>
#include <sql-parser/gda-sql-parser.h>
#include <libgda/gda-debug-macros.h>

/* "inherits" GdaVconnectionDataModelSpec */
typedef struct {
	GdaVconnectionDataModelSpec  spec;
	GdaConnection               *ldap_cnc;
	gchar                       *table_name;
	gchar                       *base_dn;
	gchar                       *filter;
	gchar                       *attributes;
	GList                       *columns;
	GdaLdapSearchScope           scope;

	GHashTable                  *filters_hash; /* key = string; value = a ComputedFilter pointer */
} LdapTableMap;

static void
_ldap_table_map_free (LdapTableMap *map)
{
	map->ldap_cnc = NULL;
	g_free (map->table_name);
	g_free (map->base_dn);
	g_free (map->filter);
	g_free (map->attributes);
	if (map->columns)
		g_list_free_full (map->columns, (GDestroyNotify) g_object_unref);
	
	if (map->filters_hash)
		g_hash_table_destroy (map->filters_hash);
	g_free (map);
}


static void gda_ldap_connection_class_init (GdaLdapConnectionClass *klass);
static void gda_ldap_connection_init       (GdaLdapConnection *cnc);
static void gda_ldap_connection_dispose    (GObject *object);
static void gda_ldap_connection_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_ldap_connection_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

typedef struct {
	GSList   *maps; /* list of #LdapTableMap, no ref held there */
	gchar    *startup_file;
	gboolean  loading_startup_file;
} GdaLdapConnectionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaLdapConnection, gda_ldap_connection, GDA_TYPE_VCONNECTION_DATA_MODEL)

/* properties */
enum
{
  PROP_0,
	PROP_STARTUP_FILE
};

static void
update_connection_startup_file (GdaLdapConnection *cnc)
{
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);
	if (! priv->startup_file || priv->loading_startup_file)
		return;

	GSList *list;
	GString *string = NULL;
	GError *lerror = NULL;

	string = g_string_new ("");
	for (list = priv->maps; list; list = list->next) {
		LdapTableMap *map = (LdapTableMap*) list->data;
		g_string_append_printf (string, "CREATE LDAP TABLE %s ", map->table_name);
		if (map->base_dn)
			g_string_append_printf (string, "BASE='%s' ", map->base_dn);
		if (map->filter)
			g_string_append_printf (string, "FILTER='%s' ", map->filter);
		if (map->attributes)
			g_string_append_printf (string, "ATTRIBUTES='%s' ", map->attributes);
		g_string_append (string, "SCOPE=");
		switch (map->scope) {
		case GDA_LDAP_SEARCH_BASE:
			g_string_append (string, "'BASE';\n");
			break;
		case GDA_LDAP_SEARCH_ONELEVEL:
			g_string_append (string, "'ONELEVEL';\n");
			break;
		case GDA_LDAP_SEARCH_SUBTREE:
			g_string_append (string, "'SUBTREE';\n");
			break;
		default:
			g_assert_not_reached ();
		}
	}
	if (! g_file_set_contents (priv->startup_file, string->str, -1, &lerror)) {
		GdaConnectionEvent *event;
		gchar *msg;
		event = gda_connection_point_available_event (GDA_CONNECTION (cnc),
							      GDA_CONNECTION_EVENT_WARNING);
		msg = g_strdup_printf (_("Error storing list of created LDAP tables: %s"),
				       lerror && lerror->message ? lerror->message : _("No detail"));
		gda_connection_event_set_description (event, msg);
		gda_connection_add_event (GDA_CONNECTION (cnc), event);
		g_free (msg);
		g_clear_error (&lerror);
	}
}

#ifdef GDA_DEBUG_NO
static void
dump_vtables (GdaLdapConnection *cnc)
{
	GSList *list;
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);
	g_print ("LDAP tables: %d\n", g_slist_length (priv->maps));
	for (list = priv->maps; list; list = list->next) {
		LdapTableMap *map = (LdapTableMap*) list->data;
		g_print ("    LDAP Vtable: %s (map %p)\n", map->table_name, map);
	}
}
#endif

static void
vtable_created (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
#ifdef GDA_DEBUG_NO
	g_print ("VTable created: %s\n", table_name);
	dump_vtables (GDA_LDAP_CONNECTION (cnc));
#endif
	if (GDA_VCONNECTION_DATA_MODEL_CLASS (gda_ldap_connection_parent_class)->vtable_created)
		GDA_VCONNECTION_DATA_MODEL_CLASS (gda_ldap_connection_parent_class)->vtable_created (cnc, table_name);
	update_connection_startup_file (GDA_LDAP_CONNECTION (cnc));
}

static void
vtable_dropped (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
	GdaLdapConnection *lcnc;
	LdapTableMap *map = NULL;
	GSList *list;

	lcnc = (GdaLdapConnection*) cnc;
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (lcnc);

	/* if @priv is NULL, then @cnc is currently being destroyed */
	if (priv) {
		for (list = priv->maps; list; list = list->next) {
			if (!strcmp (((LdapTableMap*)list->data)->table_name, table_name)) {
				map = (LdapTableMap*)list->data;
				break;
			}
		}
		if (map) {
			priv->maps = g_slist_remove (priv->maps, map);
#ifdef GDA_DEBUG_NO
			g_print ("VTable dropped: %s\n", table_name);
			dump_vtables (lcnc);
#endif
		}
	}

	if (GDA_VCONNECTION_DATA_MODEL_CLASS (gda_ldap_connection_parent_class)->vtable_dropped)
		GDA_VCONNECTION_DATA_MODEL_CLASS (gda_ldap_connection_parent_class)->vtable_dropped (cnc, table_name);

	if (priv)
		update_connection_startup_file (GDA_LDAP_CONNECTION (cnc));
}

/*
 * GdaLdapConnection class implementation
 */
static void
gda_ldap_connection_class_init (GdaLdapConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gda_ldap_connection_dispose;
	GDA_VCONNECTION_DATA_MODEL_CLASS (klass)->vtable_created = vtable_created;
	GDA_VCONNECTION_DATA_MODEL_CLASS (klass)->vtable_dropped = vtable_dropped;

	/* Properties */
  object_class->set_property = gda_ldap_connection_set_property;
  object_class->get_property = gda_ldap_connection_get_property;
	g_object_class_install_property (object_class, PROP_STARTUP_FILE,
                                         g_param_spec_string ("startup-file", NULL, _("File used to store startup data"), NULL,
                                                              (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
dsn_set_cb (GdaLdapConnection *cnc, G_GNUC_UNUSED GParamSpec *pspec, G_GNUC_UNUSED gpointer data)
{
	gchar *fname, *tmp, *dsn;
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);

	g_object_get (cnc, "dsn", &dsn, NULL);
	tmp = g_strdup_printf ("ldap-%s.start", dsn);
	g_free (dsn);
	fname = g_build_path (G_DIR_SEPARATOR_S, g_get_user_data_dir (),
			      "libgda", tmp, NULL);
	g_free (tmp);
	g_free (priv->startup_file);
	priv->startup_file = fname;
}

static void
conn_opened_cb (GdaLdapConnection *cnc, G_GNUC_UNUSED gpointer data)
{
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);
	if (!priv->startup_file)
		return;

	priv->loading_startup_file = TRUE;

	GdaSqlParser *parser;
	GdaBatch *batch;
	GError *lerror = NULL;
	parser = gda_connection_create_parser (GDA_CONNECTION (cnc));
	if (!parser)
		parser = gda_sql_parser_new ();
	batch = gda_sql_parser_parse_file_as_batch (parser, priv->startup_file, &lerror);
	if (batch) {
		GSList *list;
		list = gda_connection_batch_execute (GDA_CONNECTION (cnc), batch, NULL, 0, &lerror);
		g_slist_free_full (list, (GDestroyNotify) g_object_unref);
		g_object_unref (batch);
	}
	if (lerror) {
		GdaConnectionEvent *event;
		gchar *msg;
		event = gda_connection_point_available_event (GDA_CONNECTION (cnc),
							      GDA_CONNECTION_EVENT_WARNING);
		msg = g_strdup_printf (_("Error recreating LDAP tables: %s"),
				       lerror && lerror->message ? lerror->message : _("No detail"));
		gda_connection_event_set_description (event, msg);
		gda_connection_add_event (GDA_CONNECTION (cnc), event);
		g_free (msg);
		g_clear_error (&lerror);
	}
	g_object_unref (parser);

	priv->loading_startup_file = FALSE;
}

static void
gda_ldap_connection_init (GdaLdapConnection *cnc)
{
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);
  priv->maps = NULL;
	priv->startup_file = NULL;
	priv->loading_startup_file = FALSE;

	g_signal_connect (cnc, "notify::dsn",
			  G_CALLBACK (dsn_set_cb), NULL);
	g_signal_connect (cnc, "opened",
			  G_CALLBACK (conn_opened_cb), NULL);
}

static void
gda_ldap_connection_dispose (GObject *object)
{
	GdaLdapConnection *cnc = (GdaLdapConnection *) object;

	g_return_if_fail (GDA_IS_LDAP_CONNECTION (cnc));
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);

	/* free memory */
	if (priv->maps) {
		g_slist_free (priv->maps);
    priv->maps = NULL;
  }
  if (priv->startup_file != NULL) {
	  g_free (priv->startup_file);
    priv->startup_file = NULL;
  }

	/* chain to parent class */
	G_OBJECT_CLASS(gda_ldap_connection_parent_class)->dispose (object);
}

static void
gda_ldap_connection_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
  GdaLdapConnection *cnc;
  cnc = GDA_LDAP_CONNECTION (object);
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);
  switch (param_id) {
    case PROP_STARTUP_FILE: {
      if (priv->startup_file) {
        /* don't override any preexisting setting from a DSN */
        gchar *dsn;
        g_object_get (cnc, "dsn", &dsn, NULL);
        if (dsn) {
        g_free (dsn);
        } else {
          g_free (priv->startup_file);
          priv->startup_file = NULL;
        }
      }
      if (! priv->startup_file) {
        if (g_value_get_string (value))
          priv->startup_file = g_value_dup_string (value);
      }
      break;
    }
    default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
            break;
  }
}

static void
gda_ldap_connection_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	GdaLdapConnection *cnc;
  cnc = GDA_LDAP_CONNECTION (object);
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);
  switch (param_id) {
    case PROP_STARTUP_FILE:
			g_value_set_string (value, priv->startup_file);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void unref_objects (GObject *object, G_GNUC_UNUSED gpointer user_data) {
  g_object_unref (object);
}

static GList *
_ldap_create_columns_func (GdaVconnectionDataModelSpec *spec, G_GNUC_UNUSED GError **error)
{
	LdapTableMap *map = (LdapTableMap *) spec;
	if (!map->columns)
		map->columns = gda_data_model_ldap_compute_columns (map->ldap_cnc, map->attributes);
	g_list_foreach (map->columns, (GFunc) unref_objects, NULL);
	return g_list_copy (map->columns);
}

static gchar *
make_string_for_filter (GdaVconnectionDataModelFilter *info)
{
	GString *string;
	gint i;

	string = g_string_new ("");
	for (i = 0; i < info->nConstraint; i++) {
		const struct GdaVirtualConstraint *cons;
		cons = &(info->aConstraint [i]);
		g_string_append_printf (string, "|%d,%d", cons->iColumn, cons->op);
	}
	return g_string_free (string, FALSE);
}

typedef struct {
	gint dn_constindex; /* constraint number to set the DN from when actually executing the LDAP search, or -1 */
	gchar *ldap_filter; /* in LDAP format, with @xxx@ to bind values */
	struct GdaVirtualConstraintUsage *out_const;
} ComputedFilter;

static void
computed_filter_free (ComputedFilter *filter)
{
	g_free (filter->out_const);
	g_free (filter->ldap_filter);
	g_free (filter);
}

#define MARKER_ESCAPE_CHAR 1
#define MARKER_GLOB_CHAR 2
static void
_ldap_table_create_filter (GdaVconnectionDataModelSpec *spec, GdaVconnectionDataModelFilter *info)
{
	/*
	 * REM:
	 *   - LDAP does not allow filtering on the DN => filter only is some cases
	 *   - LDAP does not handle ordering of results => the 'ORDER BY' constraint is ignored
	 *   - the '>' and '<' operators are not allowed in search strings => using '>=' and '<=' and
	 *     have SQLite do the actual check
	 */
	LdapTableMap *map = (LdapTableMap *) spec;
	GString *filter_string = NULL;
	gint i, ncols;
	gint dn_constindex = -1;
	gchar *hash;
	
	info->orderByConsumed = FALSE;
	hash = make_string_for_filter (info);
	if (map->filters_hash) {
		ComputedFilter *filter;
		filter = g_hash_table_lookup (map->filters_hash, hash);
		if (filter) {
			info->idxPointer = (gpointer) filter;
			info->orderByConsumed = FALSE;
			memcpy (info->aConstraintUsage,
				filter->out_const,
				sizeof (struct GdaVirtualConstraintUsage) * info->nConstraint);
			/*g_print ("Reusing filter %p, hash=[%s]\n", filter, hash);*/
			g_free (hash);
			return;
		}
	}

	if (!map->columns)
		map->columns = gda_data_model_ldap_compute_columns (map->ldap_cnc, map->attributes);

	ncols = g_list_length (map->columns);
	for (i = 0; i < info->nConstraint; i++) {
		const struct GdaVirtualConstraint *cons;
		cons = &(info->aConstraint [i]);
		const gchar *attrname;

		info->aConstraintUsage[i].argvIndex = i+1;
		info->aConstraintUsage[i].omit = TRUE;

		if (cons->iColumn < 0) {
			g_warning ("Internal error: negative column number!");
			goto nofilter;
		}
		if (cons->iColumn >= ncols) {
			g_warning ("Internal error: SQLite's virtual table column %d is not known for "
				   "table '%s', which has %d column(s)", cons->iColumn, map->table_name,
				   ncols);
			goto nofilter;
		}
		if (cons->iColumn == 0) {
			/* try to optimize on the DN column */
			if ((map->scope == GDA_LDAP_SEARCH_BASE) ||
			    (map->scope == GDA_LDAP_SEARCH_ONELEVEL))
				goto nofilter;
			if (cons->op != GDA_SQL_OPERATOR_TYPE_EQ)
				goto nofilter;
			if (dn_constindex != -1) /* DN is already filtered by a constraint */
				goto nofilter;
			dn_constindex = i;
			continue;
		}

		attrname = gda_column_get_name (GDA_COLUMN (g_list_nth_data (map->columns, cons->iColumn)));
		if (! filter_string) {
			if ((info->nConstraint > 1) || map->filter)
				filter_string = g_string_new ("(& ");
			else
				filter_string = g_string_new ("");
			if (map->filter)
				g_string_append (filter_string, map->filter);
		}
		switch (cons->op) {
		case GDA_SQL_OPERATOR_TYPE_EQ:
			g_string_append_printf (filter_string, "(%s=%c)", attrname, MARKER_ESCAPE_CHAR);
			break;
		case GDA_SQL_OPERATOR_TYPE_GT:
			g_string_append_printf (filter_string, "(%s>=%c)", attrname, MARKER_ESCAPE_CHAR);
			info->aConstraintUsage[i].omit = FALSE;
			break;
		case GDA_SQL_OPERATOR_TYPE_LEQ:
			g_string_append_printf (filter_string, "(%s<=%c)", attrname, MARKER_ESCAPE_CHAR);
			info->aConstraintUsage[i].omit = FALSE;
			break;
		case GDA_SQL_OPERATOR_TYPE_LT:
			g_string_append_printf (filter_string, "(%s<=%c)", attrname, MARKER_ESCAPE_CHAR);
			break;
		case GDA_SQL_OPERATOR_TYPE_GEQ:
			g_string_append_printf (filter_string, "(%s>=%c)", attrname, MARKER_ESCAPE_CHAR);
			break;
		case GDA_SQL_OPERATOR_TYPE_REGEXP:
			g_string_append_printf (filter_string, "(%s=%c)", attrname, MARKER_GLOB_CHAR);
			break;
		default:
			/* Can't be done with LDAP */
			goto nofilter;
		}
	}

	if (!filter_string && (dn_constindex == -1))
		goto nofilter;

	if (filter_string && ((info->nConstraint > 1) || map->filter))
		g_string_append_c (filter_string, ')');
	/*g_print ("FILTER: [%s]\n", filter_string->str);*/
	
	ComputedFilter *filter;
	filter = g_new0 (ComputedFilter, 1);
	filter->dn_constindex = dn_constindex;
	if (filter_string)
		filter->ldap_filter = g_string_free (filter_string, FALSE);
	else if (map->filter)
		filter->ldap_filter = g_strdup (map->filter);
	filter->out_const = g_new (struct GdaVirtualConstraintUsage,  info->nConstraint);
	memcpy (filter->out_const,
		info->aConstraintUsage,
		sizeof (struct GdaVirtualConstraintUsage) * info->nConstraint);
	
	info->idxPointer = (gpointer) filter;
	if (! map->filters_hash)
		map->filters_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
							   g_free,
							   (GDestroyNotify) computed_filter_free);
	g_hash_table_insert (map->filters_hash, hash, filter);
	/*g_print ("There are now %d statements in store...\n", g_hash_table_size (map->filters_hash));*/
	return;

 nofilter:
	if (filter_string)
		g_string_free (filter_string, TRUE);
	for (i = 0; i < info->nConstraint; i++) {
		info->aConstraintUsage[i].argvIndex = 0;
		info->aConstraintUsage[i].omit = TRUE;
	}
	info->idxPointer = NULL;
}

static GdaDataModel *
_ldap_table_create_model_func (GdaVconnectionDataModelSpec *spec, G_GNUC_UNUSED int idxNum, const char *idxStr,
			       int argc, GValue **argv)
{
	LdapTableMap *map = (LdapTableMap *) spec;
	GdaDataModel *model;

	if (idxStr) {
		const gchar *ptr;
		gint pos;
		GString *real_filter = NULL;
		ComputedFilter *filter = (ComputedFilter *) idxStr;
		
		for (pos = 0, ptr = filter->ldap_filter ? filter->ldap_filter : ""; *ptr; ptr++) {
			if (pos == filter->dn_constindex)
				pos++; /* skip this constraint position */
			if (! real_filter)
				real_filter = g_string_new ("");
			if ((*ptr == MARKER_ESCAPE_CHAR) || (*ptr == MARKER_GLOB_CHAR)){
				gchar *str, *sptr;
				gchar marker;
				marker = *ptr;
				g_assert (pos < argc);
				str = gda_value_stringify (argv[pos]);
				for (sptr = str; *sptr; sptr++) {
					if ((*sptr == ')') || (*sptr == '(') || (*sptr == '\\') || (*sptr == '*'))
						break;
					if ((marker == MARKER_GLOB_CHAR) && (*sptr == '%'))
						break;
				}
				if (*sptr) {
					/* make the substitutions */
					GString *string;
					string = g_string_new ("");
					for (sptr = str; *sptr; sptr++) {
						if (*sptr == ')')
							g_string_append (string, "\\29");
						else if (*sptr == '(')
							g_string_append (string, "\\28");
						else if (*sptr == '\\')
							g_string_append (string, "\\5c");
						else if (*sptr == '*')
							g_string_append (string, "\\2a");
						else if ((marker == MARKER_GLOB_CHAR) && (*sptr == '%'))
							g_string_append_c (string, '*');
						else
							g_string_append_c (string, *sptr);
					}
					g_free (str);
					str = g_string_free (string, FALSE);
				}
				g_string_append (real_filter, str);
				g_free (str);
				pos++;
			}
			else
				g_string_append_c (real_filter, *ptr);
		}

		gchar *real_dn = NULL;
		GdaLdapSearchScope real_scope = map->scope;
		if (filter->dn_constindex != -1) {
			/* check that the DN is a child of the data model's base DN */
			const gchar *bdn;
			gchar *tmp;
			bdn = map->base_dn;
			if (!bdn)
				bdn = gda_ldap_connection_get_base_dn (GDA_LDAP_CONNECTION (map->ldap_cnc));
			g_assert (bdn);
			tmp = gda_value_stringify (argv[filter->dn_constindex]);
			if (g_str_has_suffix (tmp, bdn)) {
				real_scope = GDA_LDAP_SEARCH_BASE;
				real_dn = gda_value_stringify (argv[filter->dn_constindex]);
			}
			else {
				/* return empty set */
				if (real_filter)
					g_string_free (real_filter, TRUE);
				real_filter = g_string_new ("(objectClass=)");
			}
			g_free (tmp);
		}
			
		/*g_print ("FILTER to use: LDAPFilter=> [%s] LDAPDn => [%s] SCOPE => [%d]\n",
			 real_filter ? real_filter->str : NULL,
			 real_dn ? real_dn : map->base_dn, real_scope);*/
		model = GDA_DATA_MODEL (gda_data_model_ldap_new_with_config (map->ldap_cnc,
						 real_dn ? real_dn : map->base_dn,
						 real_filter ? real_filter->str : NULL,
						 map->attributes, real_scope));
		if (real_filter)
			g_string_free (real_filter, TRUE);
		g_free (real_dn);
	}
	else
		model = GDA_DATA_MODEL (gda_data_model_ldap_new_with_config (map->ldap_cnc, map->base_dn, map->filter,
						 map->attributes, map->scope));

	return model;
}

/**
 * gda_ldap_connection_declare_table:
 * @cnc: a #GdaLdapConnection
 * @table_name: a table name, not %NULL
 * @base_dn: (nullable): the base DN of the LDAP search, or %NULL for @cnc's own base DN
 * @filter: (nullable): the search filter of the LDAP search, or %NULL for a default filter of "(ObjectClass=*)"
 * @attributes: (nullable): the search attributes of the LDAP search, or %NULL if only the DN is required
 * @scope: the search scope of the LDAP search
 * @error: a place to store errors, or %NULL
 *
 * Declare a virtual table based on an LDAP search.
 *
 * The @filter argument, if not %NULL, must be a valid LDAP filter string (including the opening and
 * closing parenthesis).
 *
 * The @attribute, if not %NULL, is a list of comma separated LDAP entry attribute names. For each attribute
 * it is also possible to specify a requested #GType, and how to behave in case of multi valued attributes
 * So the general format for an attribute is:
 * "&lt;attribute name&gt;[::&lt;type&gt;][::&lt;muti value handler&gt;]", where:
 * <itemizedlist>
 * <listitem><para>"::&lt;type&gt;" is optional, see gda_g_type_from_string() for more
 *     information about valie values for &lt;type&gt;</para></listitem>
 * <listitem><para>"::&lt;muti value handler&gt;" is also optional and specifies how a multi
 *    values attributed is treated. The possibilities for &lt;muti value handler&gt; are:
 *    <itemizedlist>
 *         <listitem><para>"NULL" or "0": a NULL value will be returned</para></listitem>
 *         <listitem><para>"CSV": a comma separated value with all the values of the attribute will be
 *           returned. This only works for G_TYPE_STRING attribute types.</para></listitem>
 *         <listitem><para>"MULT" of "*": a row will be returned for each value of the attribute, effectively
 *           multiplying the number of returned rows</para></listitem>
 *         <listitem><para>"1": only the first vakue of the attribute will be used, the other values ignored</para></listitem>
 *         <listitem><para>"CONCAT": the attributes' values are concatenated (with a newline char between each value)</para></listitem>
 *         <listitem><para>"ERROR": an error value will be returned (this is the default behaviour)</para></listitem>
 *    </itemizedlist>
 * </para></listitem>
 * </itemizedlist>
 *
 *  After each attribute
 * name (and before the comma for the next attribute if any), it is possible to specify the #GType type using
 * the "::&lt;type&gt;" syntax (see gda_g_type_from_string() for more information). 
 *
 * The following example specifies the "uidNumber" attribute to be returned as a string, the "mail" attribute,
 * and the "objectClass" attribute to be handled as one row per value of that attribute:
 * <programlisting>"uidNumber::string,mail,objectClass::*"</programlisting>
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 4.2.8
 */
gboolean
gda_ldap_connection_declare_table (GdaLdapConnection *cnc, const gchar *table_name,
				   const gchar *base_dn, const gchar *filter,
				   const gchar *attributes, GdaLdapSearchScope scope,
				   GError **error)
{
	LdapTableMap *map;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);
	
	map = g_new0 (LdapTableMap, 1);
	GDA_VCONNECTION_DATA_MODEL_SPEC (map)->data_model = NULL;
        GDA_VCONNECTION_DATA_MODEL_SPEC (map)->create_columns_func = (GdaVconnectionDataModelCreateColumnsFunc) _ldap_create_columns_func;
        GDA_VCONNECTION_DATA_MODEL_SPEC (map)->create_model_func = NULL;
        GDA_VCONNECTION_DATA_MODEL_SPEC (map)->create_filter_func = _ldap_table_create_filter;
        GDA_VCONNECTION_DATA_MODEL_SPEC (map)->create_filtered_model_func = _ldap_table_create_model_func;
	map->ldap_cnc = GDA_CONNECTION (cnc); /* we can't hold a reference here because it would be self referencing,
					       * and we don't need to because the @map is destroyed when @cnc is destroyed */
	map->table_name = gda_sql_identifier_quote (table_name, GDA_CONNECTION (cnc), NULL, TRUE, FALSE);
	map->filters_hash = NULL;
	if (base_dn)
		map->base_dn = g_strdup (base_dn);
	if (filter)
		map->filter = g_strdup (filter);
	if (attributes)
		map->attributes = g_strdup (attributes);
	map->scope = scope ? scope : GDA_LDAP_SEARCH_BASE;

	priv->maps = g_slist_append (priv->maps, map);
	if (!gda_vconnection_data_model_add (GDA_VCONNECTION_DATA_MODEL (cnc),
					     (GdaVconnectionDataModelSpec*) map,
                                             (GDestroyNotify) _ldap_table_map_free, table_name, error)) {
		priv->maps = g_slist_remove (priv->maps, map);
                return FALSE;
	}

	return TRUE;
}

/**
 * gda_ldap_connection_undeclare_table:
 * @cnc: a #GdaLdapConnection
 * @table_name: a table name, not %NULL
 * @error: a place to store errors, or %NULL
 *
 * Remove a table which has been declared using gda_ldap_connection_declare_table().
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 4.2.8
 */
gboolean
gda_ldap_connection_undeclare_table (GdaLdapConnection *cnc, const gchar *table_name, GError **error)
{
	GdaVconnectionDataModelSpec *specs;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);

	specs =  gda_vconnection_data_model_get (GDA_VCONNECTION_DATA_MODEL (cnc), table_name);
	if (specs && ! g_slist_find (priv->maps, specs)) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_MISUSE_ERROR,
			     "%s", _("Can't remove non LDAP virtual table"));
		return FALSE;
	}
	return gda_vconnection_data_model_remove (GDA_VCONNECTION_DATA_MODEL (cnc), table_name, error);
}

/**
 * gda_ldap_connection_describe_table:
 * @cnc: a #GdaLdapConnection
 * @table_name: a table name, not %NULL
 * @out_base_dn: (nullable) (transfer none): a place to store the LDAP search base DN, or %NULL
 * @out_filter: (nullable) (transfer none): a place to store the LDAP search filter, or %NULL
 * @out_attributes: (nullable) (transfer none): a place to store the LDAP search attributes, or %NULL
 * @out_scope: (nullable) (transfer none): a place to store the LDAP search scope, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Get information about a virtual table, the information which has been passed to
 * gda_ldap_connection_declare_table() when the table was created.
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 4.2.8
 */
gboolean
gda_ldap_connection_describe_table (GdaLdapConnection *cnc, const gchar *table_name,
				    const gchar **out_base_dn, const gchar **out_filter,
				    const gchar **out_attributes,
				    GdaLdapSearchScope *out_scope, GError **error)
{
	GdaVconnectionDataModelSpec *specs;
	LdapTableMap *map;
  GdaLdapConnectionPrivate *priv = gda_ldap_connection_get_instance_private (cnc);

	if (out_base_dn)
		*out_base_dn = NULL;
	if (out_filter)
		*out_filter = NULL;
	if (out_attributes)
		*out_attributes = NULL;
	if (out_scope)
		*out_scope = GDA_LDAP_SEARCH_BASE;

	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	specs =  gda_vconnection_data_model_get (GDA_VCONNECTION_DATA_MODEL (cnc), table_name);
	if (specs && ! g_slist_find (priv->maps, specs)) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_MISUSE_ERROR,
			     "%s", _("Can't describe non LDAP virtual table"));
		return FALSE;
	}
	
	if (!specs) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_MISUSE_ERROR,
			     "%s", _("Unknown LDAP virtual table"));
		return FALSE;
	}

	map = (LdapTableMap*) specs;
	if (out_base_dn)
		*out_base_dn = map->base_dn;
	if (out_filter)
		*out_filter = map->filter;
	if (out_attributes)
		*out_attributes = map->attributes;
	if (out_scope)
		*out_scope = map->scope;
	return TRUE;
}

static void
gda_ldap_attribute_free (GdaLdapAttribute *attr)
{
	if (attr) {
		gint i;
		g_free (attr->attr_name);
		for (i = 0; attr->values[i]; i++)
			gda_value_free (attr->values[i]);
		g_free (attr->values);
	}
}

/**
 * gda_ldap_entry_free:
 * @entry: (transfer full): a #GdaLdapEntry pointer
 *
 * Frees @entry
 *
 * Since: 4.2.8
 */
void
gda_ldap_entry_free (GdaLdapEntry *entry)
{
	if (entry) {
		g_free (entry->dn);
		if (entry->attributes) {
			gint i;
			for (i = 0; entry->attributes[i]; i++)
				gda_ldap_attribute_free (entry->attributes[i]);
			g_free (entry->attributes);
		}
		if (entry->attributes_hash)
			g_hash_table_destroy (entry->attributes_hash);
		g_free (entry);
	}
}

/**
 * gda_ldap_entry_new:
 * @dn: (nullable): a Distinguished name, or %NULL
 *
 * Creates a new #GdaLdapEntry. This function is useful when using gda_ldap_modify_entry()
 *
 * Returns: a new #GdaLdapEntry
 *
 * Since: 5.2.0
 */
GdaLdapEntry *
gda_ldap_entry_new (const gchar *dn)
{
	GdaLdapEntry *entry;
	entry = g_new0 (GdaLdapEntry, 1);
	if (dn)
		entry->dn = g_strdup (dn);
	entry->attributes_hash = g_hash_table_new (g_str_hash, g_str_equal);
	entry->nb_attributes = 0;
	entry->attributes = g_new0 (GdaLdapAttribute*, 1);
	return entry;
}

/**
 * gda_ldap_entry_add_attribute:
 * @entry: a #GdaLdapEntry pointer
 * @merge: set to %TRUE to merge the values in case of an existing attribute in @entry, and %FALSE to replace any existing attribute's values in @entry
 * @attr_name: the name of the attribute to add
 * @nb_values: number of values in @values
 * @values: (array length=nb_values): an array of #GValue (as much values as specified by @nb_values)
 *
 * Add an attribute (ans its values) to @entry. If the attribute is already present in @entry,
 * then the attribute's values are merged or replaced depending on the @merge argument.
 *
 * Since: 5.2.0
 */
void
gda_ldap_entry_add_attribute (GdaLdapEntry *entry, gboolean merge, const gchar *attr_name,
			      guint nb_values, GValue **values)
{
	guint i;
	g_return_if_fail (entry);
	g_return_if_fail (nb_values > 0);
	g_return_if_fail (values);
	g_return_if_fail (attr_name && *attr_name);

	GdaLdapAttribute *att = NULL;
	GdaLdapAttribute *natt = NULL;
	guint replace_pos = G_MAXUINT;

	if (entry->attributes_hash)
		att = g_hash_table_lookup (entry->attributes_hash, attr_name);
	else
		entry->attributes_hash = g_hash_table_new (g_str_hash, g_str_equal);

	if (att) {
		if (merge) {
			TO_IMPLEMENT;
			return;
		}
		else {
			/* get rid of @attr */
			g_hash_table_remove (entry->attributes_hash, att->attr_name);
			for (i = 0; i < entry->nb_attributes; i++) {
				if (entry->attributes [i] == att) {
					replace_pos = i;
					entry->attributes [i] = NULL;
					break;
				}
			}
			gda_ldap_attribute_free (att);
		}
	}

	natt = g_new0 (GdaLdapAttribute, 1);
	natt->attr_name = g_strdup (attr_name);
	natt->nb_values = nb_values;
	if (natt->nb_values > 0) {
		natt->values = g_new0 (GValue *, natt->nb_values + 1);
		for (i = 0; i < natt->nb_values; i++) {
			if (values [i])
				natt->values [i] = gda_value_copy (values [i]);
			else
				natt->values [i] = NULL;
		}
	}
	g_hash_table_insert (entry->attributes_hash, natt->attr_name, natt);
	if (replace_pos != G_MAXUINT)
		entry->attributes [replace_pos] = natt;
	else {
		entry->nb_attributes++;
		entry->attributes = g_renew (GdaLdapAttribute*, entry->attributes, entry->nb_attributes + 1);
		entry->attributes [entry->nb_attributes - 1] = natt;
		entry->attributes [entry->nb_attributes] = NULL;
	}
}


/* proxy declaration */
GdaLdapEntry *_gda_ldap_describe_entry (GdaLdapConnection *cnc, const gchar *dn, GError **error);
GdaLdapEntry **_gda_ldap_get_entry_children (GdaLdapConnection *cnc, const gchar *dn, gchar **attributes, GError **error);

/**
 * gda_ldap_describe_entry:
 * @cnc: a #GdaLdapConnection connection
 * @dn: (nullable): the Distinguished Name of the LDAP entry to describe
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Describes the LDAP entry which DN is @dn. If @dn is %NULL, then the top entry (as specified when 
 * the LDAP connection was opened) is described.
 *
 * Returns: (transfer full): a new #GdaLdapEntry, or %NULL if an error occurred or if the @dn entry does not exist
 *
 * Since: 4.2.8
 */
GdaLdapEntry *
gda_ldap_describe_entry (GdaLdapConnection *cnc, const gchar *dn, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);

	return _gda_ldap_describe_entry (cnc, dn, error);
}

/**
 * gda_ldap_get_entry_children:
 * @cnc: a #GdaLdapConnection connection
 * @dn: (nullable): the Distinguished Name of the LDAP entry to get children from
 * @attributes: (nullable) (array zero-terminated=1) (element-type gchar*): a %NULL terminated array of attributes to fetch for each child, or %NULL if no attribute is requested
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Get the list of children entries for the LDAP entry which DN is @dn. If the @dn entry does not have any
 * child, then this function returns an array which first element is %NULL.
 *
 * If @dn is %NULL, then the top entry (as specified when the LDAP connection was opened) is used.
 *
 * Returns: (transfer full) (array zero-terminated=1): a %NULL terminated array of #GdaLdapEntry for each child entry, or %NULL if an error occurred or if the @dn entry does not exist
 *
 * Since: 4.2.8
 */
GdaLdapEntry **
gda_ldap_get_entry_children (GdaLdapConnection *cnc, const gchar *dn, gchar **attributes, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);

	return _gda_ldap_get_entry_children (cnc, dn, attributes, error);
}

gchar **_gda_ldap_dn_split (const gchar *dn, gboolean all);

/**
 * gda_ldap_dn_split
 * @dn: a Distinguished Name string
 * @all: set to %FALSE to split @dn into its RND and its parent DN, or %TRUE to completely split @dn
 *
 * Splits @dn into its components.
 *
 * Returns: (transfer full): a %NULL terminated array containing the DN parts (free using g_strfreev()), or %NULL if an error occurred because @dn is not a valid DN expression
 *
 * Since: 4.2.8
 */
gchar **
gda_ldap_dn_split (const gchar *dn, gboolean all)
{
	return _gda_ldap_dn_split (dn, all);
}

gboolean _gda_ldap_is_dn (const gchar *dn);

/**
 * gda_ldap_is_dn:
 * @dn: a Distinguished Name string
 *
 * Tells if @dn represents a distinguished name (it only checks for the syntax, not for
 * the actual existence of the entry with that distinguished name).
 *
 * Returns: %TRUE if @dn is a valid representation of a distinguished name
 *
 * Since: 4.2.8
 */
gboolean
gda_ldap_is_dn (const gchar *dn)
{
	return _gda_ldap_is_dn (dn);
}

const gchar *_gda_ldap_get_base_dn (GdaLdapConnection *cnc);

/**
 * gda_ldap_connection_get_base_dn:
 * @cnc: a #GdaLdapConnection
 *
 * Get the base DN which was used when the LDAP connection was opened
 *
 * Returns: the base DN, or %NULL
 *
 * Since: 4.2.8
 */
const gchar *
gda_ldap_connection_get_base_dn (GdaLdapConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);

	return _gda_ldap_get_base_dn (cnc);
}

GdaLdapClass *_gda_ldap_get_class_info (GdaLdapConnection *cnc, const gchar *classname);
/**
 * gda_ldap_get_class_info:
 * @cnc: a #GdaLdapConnection
 * @classname: an LDAP class name
 *
 * Get information about an LDAP class
 *
 * Returns: (transfer none): a #GdaLdapClass
 *
 * Since: 4.2.8
 */
GdaLdapClass*
gda_ldap_get_class_info (GdaLdapConnection *cnc, const gchar *classname)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);

	return _gda_ldap_get_class_info (cnc, classname);
}

const GSList *_gda_ldap_get_top_classes (GdaLdapConnection *cnc);
/**
 * gda_ldap_get_top_classes:
 * @cnc: a #GdaLdapConnection
 *
 * get a list of the top level LDAP classes (ie. classes which don't have any parent)
 *
 * Returns: (transfer none) (element-type GdaLdapClass): a list of #GdaLdapClass pointers (don't modify it)
 *
 * Since: 4.2.8
 */
const GSList *
gda_ldap_get_top_classes (GdaLdapConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);

	return _gda_ldap_get_top_classes (cnc);
}

GSList *_gda_ldap_entry_get_attributes_list (GdaLdapConnection *cnc, GdaLdapEntry *entry, GdaLdapAttribute *object_class_attr);
/**
 * gda_ldap_entry_get_attributes_list:
 * @cnc: a #GdaLdapConnection
 * @entry: a #GdaLdapEntry
 *
 * Get a list of all the possible attributes which @entry can have. Each possible attribute is represented
 * by a #GdaLdapAttributeDefinition strunture.
 *
 * Returns: (transfer full) (element-type GdaLdapAttributeDefinition): a #GSList of #GdaLdapAttributeDefinition pointers, free the list using gda_ldap_attributes_list_free()
 *
 * Since: 5.2.0
 */
GSList *
gda_ldap_entry_get_attributes_list (GdaLdapConnection *cnc, GdaLdapEntry *entry)
{
	g_return_val_if_fail (entry, NULL);

	return _gda_ldap_entry_get_attributes_list (cnc, entry, NULL);
}

/**
 * gda_ldap_attributes_list_free:
 * @list: (nullable): a #GSList of #GdaLdapAttributeDefinition pointers, or %NULL
 *
 * Frees the list returned by gda_ldap_entry_get_attributes_list().
 *
 * Since: 5.2.0
 */
void
gda_ldap_attributes_list_free (GSList *list)
{
	GSList *l;
	if (!list)
		return;
	for (l = list; l; l = l->next) {
		GdaLdapAttributeDefinition *def;
		def = (GdaLdapAttributeDefinition*) l->data;
		if (def) {
			g_free (def->name);
			g_free (def);
		}
	}
	g_slist_free (list);
}


gboolean
_gda_ldap_modify (GdaLdapConnection *cnc, GdaLdapModificationType modtype,
		  GdaLdapEntry *entry, GdaLdapEntry *ref_entry, GError **error);

/**
 * gda_ldap_add_entry:
 * @cnc: a #GdaLdapConnection
 * @entry: a #GdaLDapEntry describing the LDAP entry to add
 * @error: (nullable): a place to store an error, or %NULL
 *
 * Creates a new LDAP entry.
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 5.2.0
 */
gboolean
gda_ldap_add_entry (GdaLdapConnection *cnc, GdaLdapEntry *entry, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (entry, FALSE);
	g_return_val_if_fail (entry->dn && *(entry->dn), FALSE);

	return _gda_ldap_modify (cnc, GDA_LDAP_MODIFICATION_INSERT, entry, NULL, error);
}

/**
 * gda_ldap_remove_entry:
 * @cnc: a #GdaLdapConnection
 * @dn: the DN of the LDAP entry to remove
 * @error: (nullable): a place to store an error, or %NULL
 *
 * Delete an LDAP entry.
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 5.2.0
 */
gboolean
gda_ldap_remove_entry (GdaLdapConnection *cnc, const gchar *dn, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (dn && *dn, FALSE);

	GdaLdapEntry entry;
	memset (&entry, 0, sizeof (GdaLdapEntry));
	entry.dn = (gchar*) dn;

	return _gda_ldap_modify (cnc, GDA_LDAP_MODIFICATION_DELETE, &entry, NULL, error);
}

gboolean
_gda_ldap_rename_entry (GdaLdapConnection *cnc, const gchar *current_dn, const gchar *new_dn, GError **error);

/**
 * gda_ldap_rename_entry:
 * @cnc: a #GdaLdapConnection
 * @current_dn: the current DN of the entry
 * @new_dn: the new DN of the entry
 * @error: (nullable): a place to store an error, or %NULL
 *
 * Renames an LDAP entry.
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 5.2.0
 */
gboolean
gda_ldap_rename_entry (GdaLdapConnection *cnc, const gchar *current_dn, const gchar *new_dn, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (current_dn && *current_dn, FALSE);
	g_return_val_if_fail (new_dn && *new_dn, FALSE);

	return _gda_ldap_rename_entry (cnc, current_dn, new_dn, error);
}

/**
 * gda_ldap_modify_entry:
 * @cnc: a #GdaLdapConnection
 * @modtype: the type of modification to perform
 * @entry: a #GdaLDapEntry describing the LDAP entry to apply modifications, along with the attributes which will be modified
 * @ref_entry: (nullable): a #GdaLDapEntry describing the reference LDAP entry, if @modtype is %GDA_LDAP_MODIFICATION_ATTR_DIFF
 * @error: (nullable): a place to store an error, or %NULL
 *
 * Modifies an LDAP entry.
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 5.2.0
 */
gboolean
gda_ldap_modify_entry (GdaLdapConnection *cnc, GdaLdapModificationType modtype,
		       GdaLdapEntry *entry, GdaLdapEntry *ref_entry, GError **error)
{
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (entry, FALSE);
	g_return_val_if_fail (entry->dn && *(entry->dn), FALSE);

	return _gda_ldap_modify (cnc, modtype, entry, ref_entry, error);
}
