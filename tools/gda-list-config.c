#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>

static void list_all_sections (void);
static void list_all_providers (void);

gint main (int argc, char **argv) {
	gda_init ("list-sources", PACKAGE_VERSION, argc, argv);

	list_all_providers ();
	list_all_sections ();

	return 0;
}

/*
 * Lists all the sections in the config files (local to the user and global) in the index page
 */
static void list_datasource_info (gchar *dsn);
static void 
list_all_sections (void)
{
	GList *sections;
        GList *l;

	g_print ("\n=== Declared data sources ===\n");
        sections = gda_config_list_sections ("/apps/libgda/Datasources");
        for (l = sections; l != NULL; l = l->next) {
                gchar *section = (gchar *) l->data;

		g_print ("Data source: %s\n", section);

		list_datasource_info (section);
		g_print ("\n");
        }
        gda_config_free_list (sections);
}

static void
list_datasource_info (gchar *dsn)
{
	GList *keys;
	gchar *root;
	gchar *tmp;

	/* list keys in dsn */
	root = g_strdup_printf ("/apps/libgda/Datasources/%s", dsn);
	keys = gda_config_list_keys (root);
	if (keys) {
		GList *l;
		
		for (l = keys; l != NULL; l = l->next) {
			gchar *value;
			gchar *key = (gchar *) l->data;
			
			tmp = g_strdup_printf ("%s/%s", root, key);
			value = gda_config_get_string (tmp);
			g_free (tmp);
			
			g_print ("  Key '%s' = '%s'\n", key, value);
		}
		gda_config_free_list (keys);
	}
	g_free (root);
}

/*
 * make a list of all the providers in the index page
 */
static void detail_provider (const gchar *provider);
static void
list_all_providers ()
{
	GList *providers;

	g_print ("\n=== Installed providers ===\n");
	providers = gda_config_get_provider_list ();
	while (providers) {
		GdaProviderInfo *info = (GdaProviderInfo *) providers->data;
		
		detail_provider (info->id);
		g_print ("\n");

		providers = g_list_next (providers);
	}
}

static void
detail_provider (const gchar *provider)
{
	GdaProviderInfo *info;
	GSList *params;

	info = gda_config_get_provider_by_name (provider);
	if (!info)
		g_error ("Can't find provider '%s'", provider);
	
	g_print (_("Provider: %s\n"), provider);
	g_print (_("Description: %s\n"), info->description);
	g_print (_("Location: %s\n"), info->location);
	g_print (_("Data source's parameters (Name / Type / Description):\n"));

	if (info->gda_params) {
		params = info->gda_params->parameters;
		while (params) {
			gchar *tmp;

			g_object_get (G_OBJECT (params->data), "string_id", &tmp, NULL);
			g_print ("  %s / %s / %s\n", tmp,
				 gda_g_type_to_string (gda_parameter_get_g_type (GDA_PARAMETER (params->data))),
				 gda_object_get_name (GDA_OBJECT (params->data)));
			
			params = g_slist_next (params);
		}
	}
	else 
		g_print (_("None provided"));
}
