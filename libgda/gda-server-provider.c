/*
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 Holger Thon <holger.thon@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2004 - 2005 Alan Knowles <alank@src.gnome.org>
 * Copyright (C) 2004 Dani Baeyens <daniel.baeyens@hispalinux.es>
 * Copyright (C) 2004 Julio M. Merino Vidal <jmmv@menta.net>
 * Copyright (C) 2005 - 2006 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2008 - 2014 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2012, 2018 Daniel Espinosa <despinosa@src.gnome.org>
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

#undef GSEAL_ENABLE

#define G_LOG_DOMAIN "GDA-server-provider"

#include <glib.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-server-provider-private.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-util.h>
#include <libgda/gda-set.h>
#include <sql-parser/gda-sql-parser.h>
#include <gio/gio.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-lockable.h>
#include <libgda/gda-connection-private.h>
#include <libgda/gda-connection-internal.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-text.h>
#include <libgda/handlers/gda-handler-type.h>
#include <libgda/handlers/gda-handler-numerical.h>
#include "providers-support/gda-data-select-priv.h"

/*
 * provider's private information
 */
typedef struct {
	GHashTable    *data_handlers; /* key = a GdaServerProviderHandlerInfo pointer, value = a GdaDataHandler */
	GdaSqlParser  *parser;

	GHashTable    *jobs_hash; /* key = a job ID, value = a # */
	GMutex mutex;
} GdaServerProviderPrivate;

static void
gda_server_provider_iface_init (GdaLockableInterface *iface);


G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GdaServerProvider,
																	gda_server_provider,
																	G_TYPE_OBJECT,
																	G_IMPLEMENT_INTERFACE(GDA_TYPE_LOCKABLE, gda_server_provider_iface_init)
																	G_ADD_PRIVATE(GdaServerProvider))

#define CLASS(provider) (GDA_SERVER_PROVIDER_CLASS (G_OBJECT_GET_CLASS (provider)))
#define GDA_DEBUG_VIRTUAL
#undef GDA_DEBUG_VIRTUAL

static void gda_server_provider_finalize   (GObject *object);
static void gda_server_provider_constructed (GObject *object);

static void gda_server_provider_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_server_provider_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

/* properties */
enum {
        PROP_0,
};

/* module error */
GQuark gda_server_provider_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_server_provider_error");
        return quark;
}

/*
 * GdaServerProvider class implementation
 */

static void
gda_server_provider_class_init (GdaServerProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_server_provider_finalize;
	object_class->constructed = gda_server_provider_constructed;

	 /* Properties */
        object_class->set_property = gda_server_provider_set_property;
        object_class->get_property = gda_server_provider_get_property;
}

static guint
gda_server_provider_handler_info_hash_func  (GdaServerProviderHandlerInfo *key)
{
        guint hash;

        hash = g_int_hash (&(key->g_type));
        if (key->dbms_type)
                hash += g_str_hash (key->dbms_type);
        hash += GPOINTER_TO_UINT (key->cnc);

        return hash;
}

static gboolean
gda_server_provider_handler_info_equal_func (GdaServerProviderHandlerInfo *a, GdaServerProviderHandlerInfo *b)
{
        if ((a->g_type == b->g_type) &&
            (a->cnc == b->cnc) &&
            ((!a->dbms_type && !b->dbms_type) || !strcmp (a->dbms_type, b->dbms_type)))
                return TRUE;
        else
                return FALSE;
}

static void
gda_server_provider_handler_info_free (GdaServerProviderHandlerInfo *info)
{
        g_free (info->dbms_type);
        g_free (info);
}

typedef struct _WorkerData WorkerData;
static void worker_data_free (WorkerData *wd);


static void
gda_server_provider_init (GdaServerProvider *provider)
{
	g_return_if_fail (GDA_IS_SERVER_PROVIDER (provider));

	GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (provider);
	priv->data_handlers = g_hash_table_new_full ((GHashFunc) gda_server_provider_handler_info_hash_func,
							       (GEqualFunc) gda_server_provider_handler_info_equal_func,
							       (GDestroyNotify) gda_server_provider_handler_info_free,
							       (GDestroyNotify) g_object_unref);
	priv->jobs_hash = NULL;
	g_mutex_init (&priv->mutex);
}

static void
gda_server_provider_finalize (GObject *object)
{
	GdaServerProvider *provider = (GdaServerProvider *) object;
	g_return_if_fail (GDA_IS_SERVER_PROVIDER (provider));
  GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (provider);

	/* free memory */
	g_hash_table_destroy (priv->data_handlers);
	if (priv->jobs_hash)
		g_hash_table_destroy (priv->jobs_hash);
	if (priv->parser)
		g_object_unref (priv->parser);

	g_mutex_clear (&priv->mutex);

	/* chain to parent class */
	G_OBJECT_CLASS (gda_server_provider_parent_class)->finalize (object);
}

/*
 * Check that the database provider is properly implemented
 */
static void
gda_server_provider_constructed (GObject *object)
{
	GdaServerProvider *provider = (GdaServerProvider *) object;
	g_return_if_fail (GDA_IS_SERVER_PROVIDER (provider));
  GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (provider);
	const char* gtype_name = G_OBJECT_TYPE_NAME (object);

	/*g_print ("Provider %p (%s) constructed\n", provider, G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object)));*/
	if (!priv)
		g_warning ("Internal error after creation of %s: provider's private part is missing", gtype_name);
	else {
		GdaServerProviderBase *fset;
		fset = CLASS (provider)->functions_sets [GDA_SERVER_PROVIDER_FUNCTIONS_BASE];

		if (! fset)
			g_warning ("Internal provider implementation error for %s: general virtual functions are missing", gtype_name);
		else {
			if (! fset->get_name)
				g_warning ("Internal error after creation of %s: %s() virtual function missing", gtype_name, "get_name");
			if (! fset->get_version)
				g_warning ("Internal error after creation of %s: : %s() virtual function missing", gtype_name, "get_version");
			if (! fset->get_server_version)
				g_warning ("Internal error after creation of %s: : %s() virtual function missing", gtype_name, "get_server_version");
			if (!fset->supports_feature)
				g_warning ("Internal error after creation of %s: : %s() virtual function missing", gtype_name, "supports_feature");
			if (! fset->statement_to_sql)
				g_warning ("Internal error after creation of %s: : %s() virtual function missing", gtype_name, "statement_to_sql");
			if (! fset->statement_rewrite)
				g_warning ("Internal error after creation of %s: : %s() virtual function missing", gtype_name, "statement_rewrite");
			if (! fset->open_connection)
				g_warning ("Internal error after creation of %s: : %s() virtual function missing", gtype_name, "open_connection");
			if (! fset->create_worker)
				g_warning ("Internal error after creation of %s: : %s() virtual function missing", gtype_name, "create_worker");
			if (! fset->close_connection)
				g_warning ("Internal error after creation of %s: : %s() virtual function missing", gtype_name, "close_connection");
			if (fset->escape_string && !fset->unescape_string)
				g_warning ("Internal error after creation of %s: : virtual method %s implemented and %s _not_ implemented",
					   gtype_name, "escape_string()", "unescape_string()");
			else if (!fset->escape_string && fset->unescape_string)
				g_warning ("Internal error after creation of %s: : virtual method %s implemented and %s _not_ implemented",
					   gtype_name, "unescape_string()", "escape_string()");
			if (! fset->statement_prepare)
				g_warning ("Internal error after creation of %s: : %s() virtual function missing", gtype_name, "statement_prepare");
			if (! fset->statement_execute)
				g_warning ("Internal error after creation of %s: : %s() virtual function missing", gtype_name, "statement_execute");
			if (fset->begin_transaction || fset->commit_transaction || fset->rollback_transaction) {
				if (! fset->begin_transaction)
					g_warning ("Internal error after creation of %s: : %s() virtual function missing",
						   gtype_name, "begin_transaction");
				if (! fset->commit_transaction)
					g_warning ("Internal error after creation of %s: : %s() virtual function missing",
						   gtype_name, "commit_transaction");
				if (! fset->rollback_transaction)
					g_warning ("Internal error after creation of %s: : %s() virtual function missing",
						   gtype_name, "rollback_transaction");
			}
			if (fset->add_savepoint || fset->rollback_savepoint || fset->delete_savepoint) {
				if (! fset->add_savepoint)
					g_warning ("Internal error after creation of %s: : %s() virtual function missing",
						   gtype_name, "add_savepoint");
				if (! fset->rollback_savepoint)
					g_warning ("Internal error after creation of %s: : %s() virtual function missing",
						   gtype_name, "rollback_savepoint");
				if (! fset->delete_savepoint)
					g_warning ("Internal error after creation of %s: : %s() virtual function missing",
						   gtype_name, "delete_savepoint");
			}
		}

		if (GDA_IS_SERVER_PROVIDER_CLASS (gda_server_provider_parent_class)) {
			if (! CLASS (provider)->functions_sets [GDA_SERVER_PROVIDER_FUNCTIONS_META])
				CLASS (provider)->functions_sets [GDA_SERVER_PROVIDER_FUNCTIONS_META] =
					GDA_SERVER_PROVIDER_CLASS (gda_server_provider_parent_class)->functions_sets [GDA_SERVER_PROVIDER_FUNCTIONS_META];
			if (! CLASS (provider)->functions_sets [GDA_SERVER_PROVIDER_FUNCTIONS_XA])
				CLASS (provider)->functions_sets [GDA_SERVER_PROVIDER_FUNCTIONS_XA] =
					GDA_SERVER_PROVIDER_CLASS (gda_server_provider_parent_class)->functions_sets [GDA_SERVER_PROVIDER_FUNCTIONS_XA];
		}

		GdaServerProviderXa *xaset;
		xaset = CLASS (provider)->functions_sets [GDA_SERVER_PROVIDER_FUNCTIONS_XA];
		if (xaset && (xaset->xa_start || xaset->xa_end || xaset->xa_prepare || xaset->xa_commit ||
			      xaset->xa_rollback || xaset->xa_recover)) {
			if (! xaset->xa_start)
				g_warning ("Internal error after creation of %s: %s() virtual function missing",
					   gtype_name, "xa_start");
			if (! xaset->xa_end)
				g_warning ("Internal error after creation of %s: %s() virtual function missing",
					   gtype_name, "xa_end");
			if (! xaset->xa_prepare)
				g_warning ("Internal error after creation of %s: %s() virtual function missing",
					   gtype_name, "xa_prepare");
			if (! xaset->xa_commit)
				g_warning ("Internal error after creation of %s: %s() virtual function missing",
					   gtype_name ,"xa_commit");
			if (! xaset->xa_rollback)
				g_warning ("Internal error after creation of %s: %s() virtual function missing",
					   gtype_name, "xa_rollback");
			if (! xaset->xa_recover)
				g_warning ("Internal error after creation of %s: %s() virtual function missing",
					   gtype_name, "xa_recover");
		}
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_server_provider_parent_class)->constructed (object);
}


static void
gda_server_provider_set_property (GObject *object,
				  guint param_id,
				  G_GNUC_UNUSED const GValue *value,
				  GParamSpec *pspec) {

        switch (param_id) {
          default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
          break;
        }
}

static void
gda_server_provider_get_property (GObject *object,
  guint param_id,
  G_GNUC_UNUSED GValue *value,
  GParamSpec *pspec) {

  switch (param_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    break;
  }
}

static void
gda_server_provider_lock (GdaLockable *iface)
{
	GdaServerProvider *provider = GDA_SERVER_PROVIDER(iface);
	GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (provider);

	g_mutex_lock (&priv->mutex);
}

static void
gda_server_provider_unlock (GdaLockable *iface)
{
	GdaServerProvider *provider = GDA_SERVER_PROVIDER(iface);
	GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (provider);

	g_mutex_unlock (&priv->mutex);
}

static gboolean
gda_server_provider_trylock (GdaLockable *iface)
{
	GdaServerProvider *provider = GDA_SERVER_PROVIDER(iface);
	GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (provider);

	return g_mutex_trylock (&priv->mutex);
}

static void
gda_server_provider_iface_init (GdaLockableInterface *iface)
{
	iface->lock = gda_server_provider_lock;
	iface->unlock = gda_server_provider_unlock;
	iface->trylock = gda_server_provider_trylock;
}

/**
 * gda_server_provider_set_impl_functions:
 * @klass: a #GdaServerProviderClass object
 * @type: a #GdaServerProviderFunctionsType type
 * @functions_set: (nullable): a pointer to the function set, or %NULL
 *
 * Upon creation, used by provider's implementors to set the implementation functions. Passing %NULL
 * as the @functions_set has no effect.
 *
 * If some pointers of @functions_set are %NULL, they are replaced by functions from the parent class of
 * @provider.
 *
 * Warning: this function must only be called once for each different values of @type and for each @klass
 *
 * Since: 6.0
 */
void
gda_server_provider_set_impl_functions (GdaServerProviderClass *klass,
					GdaServerProviderFunctionsType type, gpointer functions_set)
{
	g_return_if_fail (GDA_IS_SERVER_PROVIDER_CLASS (klass));
	g_return_if_fail ((type >= 0) && (type < GDA_SERVER_PROVIDER_FUNCTIONS_MAX));

#ifdef GDA_DEBUG_VIRTUAL
	static GObjectClass *parent_pclass = NULL;
	parent_pclass = g_type_class_peek_parent (klass);

	g_print ("[V] %s (klass=>%p, Class=>%s, type=>%d, parent_class=>%p, parent class name=>%s )\n", __FUNCTION__,
		 klass, G_OBJECT_CLASS_NAME (klass),
		 type, parent_pclass, G_OBJECT_CLASS_NAME (parent_pclass));
#endif

	if (!functions_set)
		return;

	guint size;
	typedef void (*VirtualFunc) (void);
	switch (type) {
	case GDA_SERVER_PROVIDER_FUNCTIONS_BASE:
		size = sizeof (GdaServerProviderBase) / sizeof (VirtualFunc);
		break;
	case GDA_SERVER_PROVIDER_FUNCTIONS_META:
		size = sizeof (GdaServerProviderMeta) / sizeof (VirtualFunc);
		break;
	case GDA_SERVER_PROVIDER_FUNCTIONS_XA:
		size = sizeof (GdaServerProviderXa) / sizeof (VirtualFunc);
		break;
	default:
		g_assert_not_reached ();
	}

	guint i;
	VirtualFunc *functions;
	functions = (VirtualFunc*) klass->functions_sets [type];
	if (functions) {
		VirtualFunc *new_functions;
		new_functions = (VirtualFunc*) functions_set;
		for (i = 0; i < size; i++) {
			VirtualFunc func;
			func = new_functions [i];
			if (!func && functions [i]) {
				new_functions [i] = functions [i];
#ifdef GDA_DEBUG_VIRTUAL
				g_print ("[V] Virtual function @index %u replaced by %p\n", i, new_functions [i]);
#endif
			}
		}
	}

	klass->functions_sets [type] = functions_set;
}

/**
 * _gda_server_provider_get_impl_functions:
 * @provider: a #GdaServerProvider object
 * @worker: a #GdaWorker
 * @type: a #GdaServerProviderFunctionsType type
 *
 * Retreive the pointer to a functions set, as defined by gda_server_provider_set_impl_functions().
 *
 * Note: @worker MUST NOT BE %NULL because this function checks that it is actually called from within its worker thread.
 *
 * Returns: the pointer to the function set, or %NULL
 *
 * Since: 6.0
 */
gpointer
_gda_server_provider_get_impl_functions (GdaServerProvider *provider, GdaWorker *worker, GdaServerProviderFunctionsType type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	g_return_val_if_fail ((type >= 0) && (type < GDA_SERVER_PROVIDER_FUNCTIONS_MAX), NULL);
	g_return_val_if_fail (worker, NULL);
	g_return_val_if_fail (gda_worker_thread_is_worker (worker), NULL);

	return CLASS (provider)->functions_sets [type];
}

/*
 * gda_server_provider_get_impl_functions_for_class:
 * @provider: a #GdaServerProvider object
 * @type: a #GdaServerProviderFunctionsType type
 *
 * Reserved to database provider's implementation, to get the virtual functions of the parent implementation.
 *
 * NB: this function must only be calld from within a GdaWorker's worker thread!
 */
gpointer
gda_server_provider_get_impl_functions_for_class (GObjectClass *klass, GdaServerProviderFunctionsType type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER_CLASS (klass), NULL);
	g_return_val_if_fail ((type >= 0) && (type < GDA_SERVER_PROVIDER_FUNCTIONS_MAX), NULL);

#ifdef GDA_DEBUG_VIRTUAL
	g_print ("[V-klass] %s (klass=>%p, Class=>%s)\n", __FUNCTION__,
		 klass, G_OBJECT_CLASS_NAME (klass));
#endif

	return GDA_SERVER_PROVIDER_CLASS (klass)->functions_sets [type];
}

/*
 * _gda_server_provider_create_worker:
 * @prov: a #GdaServerProvider
 * @for_cnc: if %TRUE, then the #GdaWorker will be used for a connection, and if %FALSE, it will be used for the non connection code
 *           (in effect the returned #GdaWorker may be the same)
 *
 * Have @prov create a #GdaWorker. Any connection and C API will only be manipulated by the worker's working thread,
 * so if @prov can only be used by 1 thread, then it needs to always return the same object (increasing its reference count).
 *
 * Important Note: that this is the only code from the provider's implementation to be called by any thread.
 *
 * Returns: (transfer full): a new #GdaWorker
 */
static GdaWorker *
_gda_server_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	GdaServerProviderBase *fset;
	fset = CLASS (provider)->functions_sets [GDA_SERVER_PROVIDER_FUNCTIONS_BASE]; /* rem: we don't use
										       * _gda_server_provider_get_impl_functions()
										       * because this would fail if not
										       * called from the worker thread */
	g_assert (fset->create_worker);
	return (fset->create_worker) (provider, for_cnc);
}

/**
 * gda_server_provider_get_real_main_context:
 * @cnc: (nullable): a #GdaConnection, or %NULL
 *
 * Obtain a #GMainContext on which to iterate. This function is reserved to database provider's implementations.
 *
 * NB: if @cnc is NOT %NULL and has a #GdaWorker associated, and if we are in its worker thread, then this function
 *     returns %NULL (to avoid generating contexts which are never used)
 *
 * Returns: a #GMainContext, or %NULL. Don't forget to call g_main_context_unref() when done
 */
GMainContext *
gda_server_provider_get_real_main_context (GdaConnection *cnc)
{
	GMainContext *context;
	if (cnc) {
		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (cdata && cdata->worker && gda_worker_thread_is_worker (cdata->worker))
			return NULL;
	}

	context = gda_connection_get_main_context (cnc, NULL);
	if (context)
		g_main_context_ref (context);
	else
		context = g_main_context_new ();
	return context;
}

typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
} WorkerGetInfoData;

/* code executed in GdaWorker's worker thread */
static gpointer
worker_get_version (WorkerGetInfoData *data, G_GNUC_UNUSED GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	if (fset->get_version)
		return (gpointer) fset->get_version (data->provider);
	else
		return NULL;
}

/**
 * gda_server_provider_get_version:
 * @provider: a #GdaServerProvider object.
 *
 * Get the version of the provider.
 *
 * Returns: (transfer none): a string containing the version identification.
 */
const gchar *
gda_server_provider_get_version (GdaServerProvider *provider)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (NULL);

	GdaWorker *worker;
	worker = _gda_server_provider_create_worker (provider, FALSE);

	WorkerGetInfoData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = NULL;

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_get_version, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);
	gda_worker_unref (worker);
	return (const gchar*) retval;
}

/* code executed in GdaWorker's worker thread */
static gpointer
worker_get_name (WorkerGetInfoData *data, G_GNUC_UNUSED GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	if (fset->get_name)
		return (gpointer) fset->get_name (data->provider);
	else
		return NULL;
}

/**
 * gda_server_provider_get_name:
 * @provider: a #GdaServerProvider object.
 *
 * Get the name (identifier) of the provider
 *
 * Returns: (transfer none): a string containing the provider's name
 */
const gchar *
gda_server_provider_get_name (GdaServerProvider *provider)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (NULL);

	GdaWorker *worker;
	worker = _gda_server_provider_create_worker (provider, FALSE);

	WorkerGetInfoData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = NULL;

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_get_name, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);
	gda_worker_unref (worker);
	return (const gchar*) retval;
}

/* code executed in GdaWorker's worker thread */
static gpointer
worker_get_server_version (WorkerGetInfoData *data, G_GNUC_UNUSED GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	const gchar *retval = NULL;
	if (fset->get_server_version)
		retval = fset->get_server_version (data->provider, data->cnc);
	return (gpointer) retval;
}

/**
 * gda_server_provider_get_server_version:
 * @provider: a #GdaServerProvider object.
 * @cnc: a #GdaConnection object
 *
 * Get the version of the database to which the connection is opened.
 * 
 * Returns: (transfer none): a (read only) string, or %NULL if an error occurred
 */
const gchar *
gda_server_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerGetInfoData data;
	data.worker = cdata->worker;
	data.provider = provider;
	data.cnc = cnc;

	gpointer retval;
	gda_worker_do_job (cdata->worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_get_server_version, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	return (const gchar*) retval;
}

typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
	GdaServerOperationType type;
	GdaSet                *options;
} WorkerSupportsOperationData;

/* code executed in GdaWorker's worker thread */
static gpointer
worker_supports_operation (WorkerSupportsOperationData *data, G_GNUC_UNUSED GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	gboolean retval = FALSE;
	if (fset->supports_operation)
		retval = fset->supports_operation (data->provider, data->cnc, data->type, data->options);
	return retval ? (gpointer) 0x01 : NULL;
}

/**
 * gda_server_provider_supports_operation:
 * @provider: a #GdaServerProvider object
 * @cnc: (nullable): a #GdaConnection object which would be used to perform an action, or %NULL
 * @type: the type of operation requested
 * @options: (nullable): a list of named parameters, or %NULL
 *
 * Tells if @provider supports the @type of operation on the @cnc connection, using the
 * (optional) @options parameters.
 *
 * Returns: %TRUE if the operation is supported
 */
gboolean
gda_server_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc, 
					GdaServerOperationType type, GdaSet *options)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
		g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerSupportsOperationData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.type = type;
	data.options = options;

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_supports_operation, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

typedef struct {
	gchar                      *path;
	GdaServerOperationNodeType  node_type;
	GType                       data_type;
} OpReq;

static OpReq op_req_CREATE_DB [] = {
	{"/DB_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/DB_DEF_P/DB_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_DROP_DB [] = {
	{"/DB_DESC_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/DB_DESC_P/DB_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_CREATE_TABLE [] = {
	{"/TABLE_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/TABLE_DEF_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/FIELDS_A",                  GDA_SERVER_OPERATION_NODE_DATA_MODEL, 0},
	{"/FIELDS_A/@COLUMN_NAME",     GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN, G_TYPE_STRING},
	{"/FIELDS_A/@COLUMN_TYPE",     GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_DROP_TABLE [] = {
	{"/TABLE_DESC_P/TABLE_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_RENAME_TABLE [] = {
	{"/TABLE_DESC_P",                  GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/TABLE_DESC_P/TABLE_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/TABLE_DESC_P/TABLE_NEW_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_COMMENT_TABLE [] = {
	{"/TABLE_DESC_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/TABLE_DESC_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/TABLE_DESC_P/TABLE_COMMENT", GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_ADD_COLUMN [] = {
	{"/COLUMN_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/COLUMN_DEF_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DEF_P/COLUMN_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DEF_P/COLUMN_TYPE",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_DROP_COLUMN [] = {
	{"/COLUMN_DESC_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/COLUMN_DESC_P/TABLE_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DESC_P/COLUMN_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_COMMENT_COLUMN [] = {
	{"/COLUMN_DESC_P",                GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/COLUMN_DESC_P/TABLE_NAME",     GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DESC_P/COLUMN_NAME",    GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DESC_P/COLUMN_COMMENT", GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_RENAME_COLUMN [] = {
	{"/COLUMN_DEF_P",                 GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/COLUMN_DEF_P/TABLE_NAME",      GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DEF_P/COLUMN_NAME",     GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/COLUMN_DEF_P/COLUMN_NAME_NEW", GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_CREATE_INDEX [] = {
	{"/INDEX_DEF_P/INDEX_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/INDEX_DEF_P/INDEX_ON_TABLE",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/INDEX_FIELDS_S",               GDA_SERVER_OPERATION_NODE_SEQUENCE, 0},
	{NULL, 0, 0}
};

static OpReq op_req_RENAME_INDEX [] = {
	{"/INDEX_DEF_P/INDEX_NAME",       GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/INDEX_DEF_P/INDEX_ON_TABLE",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/INDEX_DEF_P/INDEX_NAME_NEW",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_DROP_INDEX [] = {
	{"/INDEX_DESC_P/INDEX_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_CREATE_VIEW [] = {
	{"/VIEW_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/VIEW_DEF_P/VIEW_NAME",     GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{"/VIEW_DEF_P/VIEW_DEF",      GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_DROP_VIEW [] = {
	{"/VIEW_DESC_P/VIEW_NAME",   GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

static OpReq op_req_CREATE_USER [] = {
	{"/USER_DEF_P",               GDA_SERVER_OPERATION_NODE_PARAMLIST, 0},
	{"/USER_DEF_P/USER_NAME",     GDA_SERVER_OPERATION_NODE_PARAM, G_TYPE_STRING},
	{NULL, 0, 0}
};

typedef WorkerSupportsOperationData WorkerCreateOperationData;

/* code executed in GdaWorker's worker thread */
static gpointer
worker_create_operation (WorkerCreateOperationData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	GdaServerOperation *op = NULL;
	if (fset->create_operation)
		op = fset->create_operation (data->provider, data->cnc, data->type, data->options, error);
	return (gpointer) op;
}


/**
 * gda_server_provider_create_operation:
 * @provider: a #GdaServerProvider object
 * @cnc: (nullable): a #GdaConnection object which will be used to perform an action, or %NULL
 * @type: the type of operation requested
 * @options: (nullable): a list of parameters or %NULL
 * @error: (nullable): a place to store an error, or %NULL
 *
 * Creates a new #GdaServerOperation object which can be modified in order to perform the @type type of
 * action. The @options can contain:
 * <itemizedlist>
 *  <listitem>named values which ID is a path in the resulting GdaServerOperation object, to initialize some value</listitem>
 *  <listitem>named values which may change the contents of the GdaServerOperation, see <link linkend="gda-server-op-information-std">this section</link> for more information</listitem>
 * </itemizedlist>
 *
 * Returns: (transfer full) (nullable): a new #GdaServerOperation object, or %NULL in the provider does not support the @type type of operation or if an error occurred
 */
GdaServerOperation *
gda_server_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperationType type, 
				      GdaSet *options, GError **error)
{
	static GMutex init_mutex;
	static OpReq **op_req_table = NULL;

	g_mutex_lock (&init_mutex);
	if (! op_req_table) {
		op_req_table = g_new0 (OpReq *, GDA_SERVER_OPERATION_LAST);

		op_req_table [GDA_SERVER_OPERATION_CREATE_DB] = op_req_CREATE_DB;
		op_req_table [GDA_SERVER_OPERATION_DROP_DB] = op_req_DROP_DB;
		
		op_req_table [GDA_SERVER_OPERATION_CREATE_TABLE] = op_req_CREATE_TABLE;
		op_req_table [GDA_SERVER_OPERATION_DROP_TABLE] = op_req_DROP_TABLE;
		op_req_table [GDA_SERVER_OPERATION_RENAME_TABLE] = op_req_RENAME_TABLE;

		op_req_table [GDA_SERVER_OPERATION_ADD_COLUMN] = op_req_ADD_COLUMN;
		op_req_table [GDA_SERVER_OPERATION_DROP_COLUMN] = op_req_DROP_COLUMN;
		op_req_table [GDA_SERVER_OPERATION_RENAME_COLUMN] = op_req_RENAME_COLUMN;

		op_req_table [GDA_SERVER_OPERATION_CREATE_INDEX] = op_req_CREATE_INDEX;
		op_req_table [GDA_SERVER_OPERATION_DROP_INDEX] = op_req_DROP_INDEX;
		op_req_table [GDA_SERVER_OPERATION_RENAME_INDEX] = op_req_RENAME_INDEX;

		op_req_table [GDA_SERVER_OPERATION_CREATE_VIEW] = op_req_CREATE_VIEW;
		op_req_table [GDA_SERVER_OPERATION_DROP_VIEW] = op_req_DROP_VIEW;

		op_req_table [GDA_SERVER_OPERATION_COMMENT_TABLE] = op_req_COMMENT_TABLE;
		op_req_table [GDA_SERVER_OPERATION_COMMENT_COLUMN] = op_req_COMMENT_COLUMN;

		op_req_table [GDA_SERVER_OPERATION_CREATE_USER] = op_req_CREATE_USER;
	}
	g_mutex_unlock (&init_mutex);

	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerCreateOperationData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.type = type;
	data.options = options;

	GdaServerOperation *op;
	gda_worker_do_job (worker, context, 0, (gpointer) &op, NULL,
			   (GdaWorkerFunc) worker_create_operation, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	if (op) {
		/* test op's conformance */
		OpReq *opreq = op_req_table [type];
		while (opreq && opreq->path) {
			GdaServerOperationNodeType node_type;
			node_type = gda_server_operation_get_node_type (op, opreq->path, NULL);
			if (node_type == GDA_SERVER_OPERATION_NODE_UNKNOWN) 
				g_warning (_("Provider %s created a GdaServerOperation without node for '%s'"),
					   gda_server_provider_get_name (provider), opreq->path);
			else 
				if (node_type != opreq->node_type)
					g_warning (_("Provider %s created a GdaServerOperation with wrong node type for '%s'"),
						   gda_server_provider_get_name (provider), opreq->path);
			opreq += 1;
		}

		if (options) {
			/* pre-init parameters depending on the @options argument */
			GSList *list;
			xmlNodePtr top, node;

			top =  xmlNewNode (NULL, BAD_CAST "serv_op_data");
			for (list = gda_set_get_holders (options); list; list = list->next) {
				const gchar *id;
				gchar *str = NULL;
				const GValue *value;

				id = gda_holder_get_id (GDA_HOLDER (list->data));
				value = gda_holder_get_value (GDA_HOLDER (list->data));
				if (value)
					str = gda_value_stringify (value);
				node = xmlNewTextChild (top, NULL, BAD_CAST "op_data", BAD_CAST str);
				g_free (str);
				xmlSetProp (node, BAD_CAST "path", BAD_CAST id);
			}

			if (! gda_server_operation_load_data_from_xml (op, top, error))
				g_warning ("Incorrect options");
			xmlFreeNode (top);
		}
	}

	return op;
}

typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
	GdaServerOperation    *op;
} WorkerRenderOperationData;

/* code executed in GdaWorker's worker thread */
static gpointer
worker_render_operation (WorkerRenderOperationData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	gchar *retval = NULL;
	if (fset->render_operation)
		retval = fset->render_operation (data->provider, data->cnc, data->op, error);
	return (gpointer) retval;
}

/**
 * gda_server_provider_render_operation:
 * @provider: a #GdaServerProvider object
 * @cnc: (nullable): a #GdaConnection object which will be used to render the action, or %NULL
 * @op: a #GdaServerOperation object
 * @error: (nullable): a place to store an error, or %NULL
 *
 * Creates an SQL statement (possibly using some specific extensions of the DBMS) corresponding to the
 * @op operation. Note that the returned string may actually contain more than one SQL statement.
 *
 * This function's purpose is mainly informative to get the actual SQL code which would be executed to perform
 * the operation; to actually perform the operation, use gda_server_provider_perform_operation().
 *
 * Returns: (transfer full) (nullable): a new string, or %NULL if an error occurred or operation cannot be rendered as SQL.
 */
gchar *
gda_server_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				      GdaServerOperation *op, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerRenderOperationData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.op = op;

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_render_operation, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return (gchar*) retval;
}

typedef struct {
	GdaWorker          *worker;
	GdaServerProvider  *provider;
	GdaConnection      *cnc;
	GdaServerOperation *op;
} WorkerPerformOperationData;

/* code executed in GdaWorker's worker thread */
static gpointer
worker_perform_operation (WorkerPerformOperationData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	gboolean result;
	if (fset->perform_operation)
		result = fset->perform_operation (data->provider, data->cnc, data->op, error);
	else 
		result = gda_server_provider_perform_operation_default (data->provider, data->cnc, data->op, error);
	return result ? (gpointer) 0x01 : NULL;
}

/**
 * gda_server_provider_perform_operation:
 * @provider: a #GdaServerProvider object
 * @cnc: (nullable): a #GdaConnection object which will be used to perform the action, or %NULL
 * @op: a #GdaServerOperation object
 * @error: (nullable): a place to store an error, or %NULL
 *
 * Performs the operation described by @op. Note that @op is not destroyed by this method
 * and can be reused.
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_server_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc, 
				       GdaServerOperation *op, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
		g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerPerformOperationData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.op = op;

#ifdef GDA_DEBUG_NO
	{
		g_print ("Perform GdaServerOperation:\n");
		xmlNodePtr node;
		node = gda_server_operation_save_data_to_xml (op, NULL);
		xmlDocPtr doc;
		doc = xmlNewDoc ("1.0");
		xmlDocSetRootElement (doc, node);
		xmlDocDump (stdout, doc);
		xmlFreeDoc (doc);
	}
#endif

	gpointer retval = NULL;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_perform_operation, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

typedef struct {
	GdaWorker           *worker;
	GdaServerProvider   *provider;
	GdaConnection       *cnc;
	GdaConnectionFeature feature;
} WorkerSupportsFeatureData;

/* code executed in GdaWorker's worker thread */
static gpointer
worker_supports_feature (WorkerSupportsFeatureData *data, G_GNUC_UNUSED GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	gboolean result = FALSE;
	if (fset->supports_feature) {
		result = fset->supports_feature (data->provider, data->cnc, data->feature);
		if (result && (data->feature == GDA_CONNECTION_FEATURE_XA_TRANSACTIONS)) {
			GdaServerProviderXa *xaset;
			xaset = _gda_server_provider_get_impl_functions (data->provider, data->worker,
									 GDA_SERVER_PROVIDER_FUNCTIONS_XA);
			if (!xaset->xa_start || !xaset->xa_end || !xaset->xa_prepare || !xaset->xa_commit ||
			    !xaset->xa_rollback || !xaset->xa_recover)
				result = FALSE;
		}
	}
	return result ? (gpointer) 0x01 : NULL;
}

/**
 * gda_server_provider_supports_feature:
 * @provider: a #GdaServerProvider object
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @feature: #GdaConnectionFeature feature to test
 *
 * Tests if a feature is supported
 *
 * Returns: %TRUE if @feature is supported
 */
gboolean
gda_server_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaConnectionFeature feature)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
		g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerSupportsFeatureData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.feature = feature;

	gpointer retval = NULL;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_supports_feature, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

typedef struct {
	GdaWorker           *worker;
	GdaServerProvider   *provider;
	GdaConnection       *cnc;
	GType                for_g_type;
	const gchar         *for_dbms_type;
} WorkerTypeData;

/* code executed in GdaWorker's worker thread */
static gpointer
worker_get_data_handler (WorkerTypeData *data, G_GNUC_UNUSED GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	GdaDataHandler *dh = NULL;
	if (fset->get_data_handler)
		dh = fset->get_data_handler (data->provider, data->cnc, data->for_g_type, data->for_dbms_type);
	return (gpointer) dh;
}

/**
 * gda_server_provider_get_data_handler_g_type:
 * @provider: a server provider.
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @for_type: a #GType
 *
 * Find a #GdaDataHandler object to manipulate data of type @for_type. The returned object must not be modified.
 * 
 * Returns: (transfer none): a #GdaDataHandler, or %NULL if the provider does not support the requested @for_type data type 
 */
GdaDataHandler *
gda_server_provider_get_data_handler_g_type (GdaServerProvider *provider, GdaConnection *cnc, GType for_type)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerTypeData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.for_g_type = for_type;
	data.for_dbms_type = NULL;

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_get_data_handler, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return (GdaDataHandler*) retval;
}

/**
 * gda_server_provider_get_data_handler_dbms:
 * @provider: a server provider.
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @for_type: a DBMS type definition
 *
 * Find a #GdaDataHandler object to manipulate data of type @for_type.
 *
 * Note: this function is currently very poorly implemented by database providers.
 * 
 * Returns: (transfer none): a #GdaDataHandler, or %NULL if the provider does not know about the @for_type type
 */
GdaDataHandler *
gda_server_provider_get_data_handler_dbms (GdaServerProvider *provider, GdaConnection *cnc, const gchar *for_type)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerTypeData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.for_g_type = G_TYPE_INVALID;
	data.for_dbms_type = for_type;

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_get_data_handler, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return (GdaDataHandler*) retval;
}

/* code executed in GdaWorker's worker thread */
static gpointer
worker_get_default_dbms_type (WorkerTypeData *data, G_GNUC_UNUSED GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	const gchar *def = NULL;
	if (fset->get_def_dbms_type)
		def = fset->get_def_dbms_type (data->provider, data->cnc, data->for_g_type);
	return (gpointer) def;
}

/**
 * gda_server_provider_get_default_dbms_type:
 * @provider: a server provider.
 * @cnc: (nullable):  a #GdaConnection object or %NULL
 * @type: a #GType value type
 *
 * Get the name of the most common data type which has @type type.
 *
 * The returned value may be %NULL either if the provider does not implement that method, or if
 * there is no DBMS data type which could contain data of the @g_type type (for example %NULL may be
 * returned if a DBMS has integers only up to 4 bytes and a #G_TYPE_INT64 is requested).
 *
 * Returns: (transfer none) (nullable): the name of the DBMS type, or %NULL
 */
const gchar *
gda_server_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc, GType type)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning (_("Internal error: connection reported as opened, yet no provider's data has been setted"));
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerTypeData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.for_g_type = type;
	data.for_dbms_type = NULL;

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_get_default_dbms_type, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return (const gchar*) retval;
}


/**
 * gda_server_provider_string_to_value:
 * @provider: a server provider.
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @string: the SQL string to convert to a value
 * @preferred_type: a #GType, or #G_TYPE_INVALID
 * @dbms_type: (nullable): place to get the actual database type used if the conversion succeeded, or %NULL
 *
 * Use @provider to create a new #GValue from a single string representation. 
 *
 * The @preferred_type can optionally ask @provider to return a #GValue of the requested type 
 * (but if such a value can't be created from @string, then %NULL is returned); 
 * pass #G_TYPE_INVALID if any returned type is acceptable.
 *
 * The returned value is either a new #GValue or %NULL in the following cases:
 * - @string cannot be converted to @preferred_type type
 * - the provider does not handle @preferred_type
 * - the provider could not make a #GValue from @string
 *
 * If @dbms_type is not %NULL, then if will contain a constant string representing
 * the database type used for the conversion if the conversion was successfull, or %NULL
 * otherwise.
 *
 * Returns: (transfer full): a new #GValue, or %NULL
 */
GValue *
gda_server_provider_string_to_value (GdaServerProvider *provider, GdaConnection *cnc, const gchar *string, 
				     GType preferred_type, gchar **dbms_type)
{
	GValue *retval = NULL;
	GdaDataHandler *dh;
	gsize i;

	if (dbms_type)
		*dbms_type = NULL;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		gda_lockable_lock ((GdaLockable*) cnc);
	}

	if (preferred_type != G_TYPE_INVALID) {
		dh = gda_server_provider_get_data_handler_g_type (provider, cnc, preferred_type);
		if (dh) {
			retval = gda_data_handler_get_value_from_sql (dh, string, preferred_type);
			if (retval) {
				gchar *tmp;
				
				tmp = gda_data_handler_get_sql_from_value (dh, retval);
				if (!tmp || strcmp (tmp, string)) {
					gda_value_free (retval);
					retval = NULL;
				}
				else {
					if (dbms_type)
						*dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (provider, 
														  cnc, preferred_type);
				}
				
				g_free (tmp);
			}
		}
	}
	else {
		/* test all the possible data types and see if we have a match */
		GType types[] = {G_TYPE_UCHAR,
				 GDA_TYPE_USHORT,
				 G_TYPE_UINT,
				 G_TYPE_UINT64,
				 
				 G_TYPE_CHAR,
				 GDA_TYPE_SHORT,
				 G_TYPE_INT,
				 G_TYPE_INT64,
				 
				 G_TYPE_FLOAT,
				 G_TYPE_DOUBLE,
				 GDA_TYPE_NUMERIC,
				 
				 G_TYPE_BOOLEAN,
				 GDA_TYPE_TIME,
				 G_TYPE_DATE,
				 G_TYPE_DATE_TIME,
				 GDA_TYPE_GEOMETRIC_POINT,
				 G_TYPE_STRING,
				 GDA_TYPE_BINARY};
		
		for (i = 0; !retval && (i <= (sizeof(types)/sizeof (GType)) - 1); i++) {
			dh = gda_server_provider_get_data_handler_g_type (provider, cnc, types [i]);
			if (dh) {
				retval = gda_data_handler_get_value_from_sql (dh, string, types [i]);
				if (retval) {
					gchar *tmp;
					
					tmp = gda_data_handler_get_sql_from_value (dh, retval);
					if (!tmp || strcmp (tmp, string)) {
						gda_value_free (retval);
						retval = NULL;
					}
					else {
						if (dbms_type)
							*dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (provider, 
															  cnc, types[i]);
					}
					g_free (tmp);
				}
			}
		}
	}
	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc);

	return retval;
}

 
/**
 * gda_server_provider_value_to_sql_string:
 * @provider: a server provider.
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @from: #GValue to convert from
 *
 * Produces a fully quoted and escaped string from a GValue
 *
 * Returns: (transfer full): escaped and quoted value or NULL if not supported.
 */
gchar *
gda_server_provider_value_to_sql_string (GdaServerProvider *provider,
					 GdaConnection *cnc,
					 GValue *from)
{
	gchar *retval = NULL;
	GdaDataHandler *dh;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (!cnc || (gda_connection_get_provider (cnc) == provider), NULL);
	g_return_val_if_fail (from != NULL, NULL);

	if (cnc)
		gda_lockable_lock ((GdaLockable*) cnc);
	dh = gda_server_provider_get_data_handler_g_type (provider, cnc, G_VALUE_TYPE (from));
	if (dh)
		retval = gda_data_handler_get_sql_from_value (dh, from);
	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc);
	return retval;
}

typedef struct {
	GdaWorker *worker;
	GdaServerProvider *provider;
	GdaConnection *cnc;
	const gchar *str;
} WorkerEscapeData;

static gpointer
worker_escape_string (WorkerEscapeData *data, G_GNUC_UNUSED GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	if (fset->escape_string)
		return fset->escape_string (data->provider, data->cnc, data->str);
	else
		return gda_default_escape_string (data->str);
}

/**
 * gda_server_provider_escape_string:
 * @provider: a server provider.
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @str: a string to escape
 *
 * Escapes @str for use within an SQL command (to avoid SQL injection attacks). Note that the returned value still needs
 * to be enclosed in single quotes before being used in an SQL statement.
 *
 * Returns: (transfer full): a new string suitable to use in SQL statements
 */
gchar *
gda_server_provider_escape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (str, NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerEscapeData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.str = str;

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_escape_string, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return (gchar*) retval;
}

static gpointer
worker_unescape_string (WorkerEscapeData *data, G_GNUC_UNUSED GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	if (fset->unescape_string)
		return fset->unescape_string (data->provider, data->cnc, data->str);
	else
		return gda_default_unescape_string (data->str);
}

/**
 * gda_server_provider_unescape_string:
 * @provider: a server provider.
 * @cnc: (nullable): a #GdaConnection object, or %NULL
 * @str: a string to escape
 *
 * Unescapes @str for use within an SQL command. This is the exact opposite of gda_server_provider_escape_string().
 *
 * Returns: (transfer full): a new string
 */
gchar *
gda_server_provider_unescape_string (GdaServerProvider *provider, GdaConnection *cnc, const gchar *str)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (str, NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerEscapeData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.str = str;

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_unescape_string, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return (gchar*) retval;
}

typedef struct {
	GdaWorker *worker;
	GdaServerProvider *provider;
	GdaConnection *cnc;
} WorkerParserData;

static gpointer
worker_create_parser (WorkerParserData *data, G_GNUC_UNUSED GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	if (fset->create_parser)
		return fset->create_parser (data->provider, data->cnc);
	else
		return NULL;
}

/**
 * gda_server_provider_create_parser:
 * @provider: a #GdaServerProvider provider object
 * @cnc: (nullable): a #GdaConnection, or %NULL
 *
 * Creates a new #GdaSqlParser object which is adapted to @provider (and possibly depending on
 * @cnc for the actual database version).
 *
 * If @prov does not have its own parser, then %NULL is returned, and a general SQL parser can be obtained
 * using gda_sql_parser_new().
 *
 * Returns: (transfer full): a new #GdaSqlParser object, or %NULL.
 */
GdaSqlParser *
gda_server_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerParserData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;

	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_create_parser, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc)
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return (GdaSqlParser*) retval;
}

/*
 * Possible job types
 */
typedef enum {
	JOB_CREATE_CONNECTION,
	JOB_OPEN_CONNECTION,
	JOB_CLOSE_CONNECTION,
	JOB_PREPARE_STATEMENT,
	JOB_EXECUTE_STATEMENT,
	JOB_IDENTIFIER_QUOTE,
	JOB_META,
	JOB_BEGIN_TRANSACTION,
	JOB_COMMIT_TRANSACTION,
	JOB_ROLLBACK_TRANSACTION,
	JOB_ADD_SAVEPOINT,
	JOB_ROLLBACK_SAVEPOINT,
	JOB_DELETE_SAVEPOINT,
	JOB_XA_START,
	JOB_XA_END,
	JOB_XA_PREPARE,
	JOB_XA_COMMIT,
	JOB_XA_ROLLBACK,
	JOB_XA_RECOVER,
	JOB_STMT_TO_SQL,
} WorkerDataType;

/*
 * Holds information associated with any kind of job
 */
struct _WorkerData {
	guint              job_id;
	WorkerDataType     job_type;

	gpointer           job_data;
	GDestroyNotify     job_data_destroy_func;
};

/*
 * generic function to free the job's associated data
 */
static void
worker_data_free (WorkerData *wd)
{
	if (wd->job_data) {
		g_assert (wd->job_data_destroy_func);
		wd->job_data_destroy_func (wd->job_data);
	}
	g_slice_free (WorkerData, wd);
}

static void server_provider_job_done_callback (GdaWorker *worker, guint job_id, gpointer result,
					       GError *error, GdaServerProvider *provider);

/***********************************************************************************************************/

/*
 *   JOB_CREATE_CONNECTION
 */

typedef struct {
	GdaWorker *worker;
	GdaServerProvider *provider;
} WorkerCreateConnectionData;

static gpointer
worker_create_connection (WorkerCreateConnectionData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	if (fset->create_connection)
		return fset->create_connection (data->provider);
	else
		return NULL;
}

GdaConnection *
_gda_server_provider_create_connection (GdaServerProvider *provider, const gchar *dsn_string, const gchar *cnc_string,
					const gchar *auth_string, GdaConnectionOptions options)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (!dsn_string || !cnc_string, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (NULL);

	GdaWorker *worker;
	worker = _gda_server_provider_create_worker (provider, FALSE);

	WorkerCreateConnectionData data;
	data.provider = provider;
	data.worker = worker;

	gpointer cnc;
	gda_worker_do_job (worker, context, 0, &cnc, NULL,
			   (GdaWorkerFunc) worker_create_connection, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);
	gda_worker_unref (worker);

	if (cnc)
		g_object_set (G_OBJECT (cnc),
			      "provider", provider,
			      "auth-string", auth_string,
			      "options", options, NULL);
	else
		cnc =  g_object_new (GDA_TYPE_CONNECTION,
				     "provider", provider,
				     "auth-string", auth_string,
				     "options", options, NULL);

	if (dsn_string)
		g_object_set (G_OBJECT (cnc),
			      "dsn", dsn_string, NULL);
	else if (cnc_string)
		g_object_set (G_OBJECT (cnc),
			      "cnc-string", cnc_string, NULL);
	return (GdaConnection*) cnc;
}

/***********************************************************************************************************/

/*
 *   JOB_OPEN_CONNECTION
 *   WorkerOpenConnectionData
 *
 */
typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
	GdaQuarkList          *params;
	GdaQuarkList          *auth;
	GdaConnectionOpenFunc  callback;
	gpointer               callback_data; /* FIXME: add data destroy func */
} WorkerOpenConnectionData;

static void
WorkerOpenConnectionData_free (WorkerOpenConnectionData *data)
{
	gda_worker_unref (data->worker);
	g_object_unref (data->cnc);	
	gda_quark_list_free (data->params);
	gda_quark_list_free (data->auth);
	g_slice_free (WorkerOpenConnectionData, data);
}

static void
compute_error (GdaConnection *cnc, GError **error)
{
	if (!error)
		return;
	const GList *events, *l;
	events = gda_connection_get_events (cnc);

	for (l = g_list_last ((GList*) events); l; l = l->prev) {
		GdaConnectionEvent *event;

		event = GDA_CONNECTION_EVENT (l->data);
		if (gda_connection_event_get_event_type (event) == GDA_CONNECTION_EVENT_ERROR) {
			if (!(*error))
				g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_OPEN_ERROR,
					     "%s", gda_connection_event_get_description (event));
		}
	}
}

static gpointer
worker_open_connection (WorkerOpenConnectionData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	guint delay;
	delay = _gda_connection_get_exec_slowdown (data->cnc);
	if (delay > 0) {
		g_print (_("Delaying opening connection for %u ms\n"), delay);
		g_usleep (delay);
	}

	gboolean result;
	result = fset->open_connection (data->provider, data->cnc, data->params, data->auth);
	if (result) {
		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (data->cnc, NULL);
		if (!cdata) {
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			result = FALSE;
		}
		else
			cdata->worker = gda_worker_ref (data->worker);

		if (fset->prepare_connection) {
			result = fset->prepare_connection (data->provider, data->cnc, data->params, data->auth);
			if (!result) {
				compute_error (data->cnc, error);

				fset->close_connection (data->provider, data->cnc);
				gda_connection_internal_set_provider_data (data->cnc, NULL, NULL);

				gda_worker_unref (cdata->worker);
				cdata->worker = NULL;

				if (cdata->provider_data_destroy_func)
					cdata->provider_data_destroy_func (cdata);
			}
		}
	}
	if (data->params)
		gda_quark_list_protect_values (data->params);
	if (data->auth)
		gda_quark_list_protect_values (data->params);

	/* error computing */
	if (!result)
		compute_error (data->cnc, error);

	return result ? (gpointer) 0x01 : NULL;
}

/*
 * Steals @worker
 */
static gboolean
stage2_open_connection (GdaWorker *worker, GdaConnection *cnc, gpointer result)
{
	if (result) {
		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			result = NULL;
			_gda_connection_set_status (cnc, GDA_CONNECTION_STATUS_CLOSED);
		}
		else {
			_gda_connection_set_status (cnc, GDA_CONNECTION_STATUS_IDLE);
			g_signal_emit_by_name (G_OBJECT (cnc), "opened");
		}
	}

	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return result ? TRUE : FALSE;
}

/*
 * _gda_server_provider_open_connection:
 * @provider: a #GdaServerProvider
 * @cnc: a #GdaConnection
 * @params:  (transfer full): parameters specifying the connection's attributes
 * @auth: (nullable) (transfer full): authentification parameters, or %NULL
 * @cb_func: (nullable): a #GdaConnectionOpenFunc function, or %NULL
 * @data: (nullable): data to pass to @cb_func, or %NULL
 * @out_job_id: (nullable): a place to store the job ID, or %NULL
 * @error: (nullable): a place to store error, or %NULL
 *
 * Call the open_connection() in the worker thread.
 *
 * 2 modes:
 *  - sync: where @cb_func and @out_job_id are _both_ NULL, and @data is ignored
 *    Returns: %FALSE if an error occurred, and %TRUE if connection is then opened
 *
 *  - async: where @cb_func and @out_job_id are _both_ NOT NULL, and @error is ignored
 *    Returns: %FALSE if an error occurred submitting the job, and %TRUE if job has been submitted. @error may contain the
 *             error when submitting the job
 *
 */
gboolean
_gda_server_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaQuarkList *params, GdaQuarkList *auth,
				      GdaConnectionOpenFunc cb_func, gpointer data, guint *out_job_id,
				      GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (cnc && (gda_connection_get_provider (cnc) == provider), FALSE);
	g_return_val_if_fail (! gda_connection_is_opened (cnc), TRUE);
	g_return_val_if_fail (params, FALSE);

  GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (provider);

	if (out_job_id)
		*out_job_id = 0;
	g_return_val_if_fail ((cb_func && out_job_id) || (!cb_func && !out_job_id), FALSE);

	GMainContext *context;
	context = gda_connection_get_main_context (cnc, NULL);
	if (cb_func && !context) {
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_NO_MAIN_CONTEXT_ERROR,
			     "%s", _("You need to define a GMainContext using gda_connection_set_main_context()"));
		return FALSE;
	}

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */
	_gda_connection_set_status (cnc, GDA_CONNECTION_STATUS_OPENING);

	GdaWorker *worker;
	worker = _gda_server_provider_create_worker (provider, TRUE);
	_gda_connection_internal_set_worker_thread (cnc, gda_worker_get_worker_thread (worker));

	/* define callback if not yet done */
	if (cb_func) {
		if (!gda_worker_set_callback (worker, context,
					      (GdaWorkerCallback) server_provider_job_done_callback, provider, error)) {
			_gda_connection_set_status (cnc, GDA_CONNECTION_STATUS_CLOSED);
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			gda_worker_unref (worker);
			return FALSE;
		}

		if (!priv->jobs_hash)
			priv->jobs_hash = g_hash_table_new_full (g_int_hash, g_int_equal,
									   NULL, (GDestroyNotify) worker_data_free);
	}

	/* preparing data required to submit job to GdaWorker */
	WorkerOpenConnectionData *jdata;
	jdata = g_slice_new (WorkerOpenConnectionData);
	jdata->worker = gda_worker_ref (worker);
	jdata->provider = provider;
	jdata->cnc = g_object_ref (cnc);
	jdata->params = params;
	jdata->auth = auth;
	jdata->callback = cb_func;
	jdata->callback_data = data;

	if (cb_func) {
		guint job_id;
		job_id = gda_worker_submit_job (worker, context,
						(GdaWorkerFunc) worker_open_connection,
						jdata, NULL, NULL, error);
		if (job_id == 0) {
			_gda_connection_set_status (cnc, GDA_CONNECTION_STATUS_CLOSED);
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			WorkerOpenConnectionData_free (jdata);
			return FALSE; /* error */
		}
		else {
			WorkerData *wd;
			wd = g_slice_new0 (WorkerData);
			wd->job_id = job_id;
			wd->job_type = JOB_OPEN_CONNECTION;
			wd->job_data = (gpointer) jdata;
			wd->job_data_destroy_func = (GDestroyNotify) WorkerOpenConnectionData_free;
			g_hash_table_insert (priv->jobs_hash, & wd->job_id, wd);
			*out_job_id = job_id;
			return TRUE; /* no error, LOCK on CNC is kept */
		}
	}
	else {
		gpointer result;
		if (context)
			g_main_context_ref (context);
		else
			context = g_main_context_new ();
		gda_worker_do_job (worker, context, 0, &result, NULL,
				   (GdaWorkerFunc) worker_open_connection, jdata, (GDestroyNotify) WorkerOpenConnectionData_free,
				   NULL, error);
		g_main_context_unref (context);

		gboolean retval;
		retval = stage2_open_connection (worker, cnc, result); /* steals @worker and unlocks @cnc */
		return retval; 
	}
}

/***********************************************************************************************************/

/*
 *   JOB_CLOSE_CONNECTION
 *   WorkerCloseConnectionData
 *
 */
typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
} WorkerCloseConnectionData;

static void
WorkerCloseConnectionData_free (WorkerCloseConnectionData *data)
{
	//g_print ("%s() th %p %s\n", __FUNCTION__, g_thread_self(), gda_worker_thread_is_worker (data->worker) ? "Thread Worker" : "NOT thread worker");
	_gda_connection_internal_set_worker_thread (data->cnc, NULL);
	gda_worker_unref (data->worker);
	g_object_unref (data->cnc);
	g_slice_free (WorkerCloseConnectionData, data);
}

static gpointer
worker_close_connection (WorkerCloseConnectionData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	gboolean result;
	result = fset->close_connection (data->provider, data->cnc);

	guint delay;
	delay = _gda_connection_get_exec_slowdown (data->cnc);
	if (delay > 0) {
		g_print (_("Delaying closing connection for %u ms\n"), delay / 1000);
		g_usleep (delay);
	}

	/* error computing */
	if (!result) {
		const GList *events, *l;
		events = gda_connection_get_events (data->cnc);

		for (l = g_list_last ((GList*) events); l; l = l->prev) {
			GdaConnectionEvent *event;

			event = GDA_CONNECTION_EVENT (l->data);
			if (gda_connection_event_get_event_type (event) == GDA_CONNECTION_EVENT_ERROR) {
				if (error && !(*error))
					g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_OPEN_ERROR,
						     "%s", gda_connection_event_get_description (event));
			}
		}
	}

	return result ? (gpointer) 0x01 : NULL;
}

static gboolean
stage2_close_connection (GdaConnection *cnc, gpointer result)
{
	if (result) {
		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (cdata) {
			gda_connection_internal_set_provider_data (cnc, NULL, NULL);

			if (cdata->provider_data_destroy_func) {
				cdata->provider_data_destroy_func (cdata);
			}
		}
		_gda_connection_set_status (cnc, GDA_CONNECTION_STATUS_CLOSED);
	}

	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	return result ? TRUE : FALSE;
}

/*
 * _gda_server_provider_close_connection:
 * @provider: a #GdaServerProvider
 * @cnc: a #GdaConnection
 * @error: (nullable): a place to store error, or %NULL
 *
 * Call the close_connection() in the worker thread.
 */
gboolean
_gda_server_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc, GError **error)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (cnc && (gda_connection_get_provider (cnc) == provider), FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), TRUE);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerCloseConnectionData *jdata;
	jdata = g_slice_new (WorkerCloseConnectionData);
	jdata->worker = gda_worker_ref (cdata->worker);
	jdata->provider = provider;
	jdata->cnc = g_object_ref (cnc);

	GdaWorker *worker;
	worker = cdata->worker;

	gpointer result;
	gda_worker_do_job (cdata->worker, context, 0, &result, NULL,
			   (GdaWorkerFunc) worker_close_connection, jdata, (GDestroyNotify) WorkerCloseConnectionData_free,
			   NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_worker_unref (worker);

	return stage2_close_connection (cnc, result);
}

/***********************************************************************************************************/

/*
 *   JOB_PREPARE_STATEMENT
 *   WorkerPrepareStatementData
 *
 */
typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
	GdaStatement          *stmt;
} WorkerPrepareStatementData;

/* code executed in GdaWorker's worker thread */
static gpointer
worker_statement_prepare (WorkerPrepareStatementData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	gboolean result;
	result = fset->statement_prepare (data->provider, data->cnc, data->stmt, error);
	return result ? (gpointer) 0x01 : NULL;
}

/*
 * _gda_server_provider_statement_prepare:
 * @provider: a #GdaServerProvider
 * @cnc: (nullable): a #GdaConnection
 * @stmt: a #GdaStatement
 * @error: (nullable): a place to store error, or %NULL
 *
 * Call the prepare_statement() in the worker thread
 */
gboolean
_gda_server_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
					GdaStatement *stmt, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}
	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerPrepareStatementData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.stmt = stmt;

	gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_statement_prepare, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage (cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

/***********************************************************************************************************/

/*
 *   JOB_EXECUTE_STATEMENT
 *   WorkerExecuteStatementData
 *
 */
typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
	GdaStatement          *stmt;
	GdaSet                *params;
	GdaStatementModelUsage model_usage;
	GType                 *col_types;
	GdaSet               **last_inserted_row;
} WorkerExecuteStatementData;

static gpointer
worker_statement_execute (WorkerExecuteStatementData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	guint delay;
	delay = _gda_connection_get_exec_slowdown (data->cnc);
	if (delay > 0) {
		g_print (_("Delaying statement execution for %u ms\n"), delay / 1000);
		g_usleep (delay);
	}

	GObject *result;
	result = fset->statement_execute (data->provider, data->cnc, data->stmt, data->params, data->model_usage,
					  data->col_types, data->last_inserted_row, error);

	if (GDA_IS_DATA_SELECT (result)) {
		/* adjust flags because the providers don't necessarily do it: make sure extra flags as OFFLINE and
		 * ALLOW_NOPARAM are included */
  	data->model_usage = data->model_usage & (GDA_STATEMENT_MODEL_OFFLINE | GDA_STATEMENT_MODEL_ALLOW_NOPARAM);

		/* if necessary honor the OFFLINE flag */
		if ((data->model_usage & GDA_STATEMENT_MODEL_OFFLINE) &&
		    ! gda_data_select_prepare_for_offline ((GdaDataSelect*) result, error)) {
			g_object_unref (result);
			result = NULL;
		}
	}

	return (gpointer) result;
}

/*
 * _gda_server_provider_statement_execute:
 * @provider: a #GdaServerProvider
 * @cnc: a #GdaConnection
 * @stmt: a #GdaStatement
 * @params: (nullable): parameters to bind variables in @stmt, or %NULL
 * @model_usage: the requested usage of the returned #GdaDataModel if @stmt is a SELECT statement
 * @col_types: (nullable): requested column types of the returned #GdaDataModel if @stmt is a SELECT statement, or %NULL
 * @last_inserted_row: (nullable): a place to store the last inserted row information, or %NULL
 * @error: (nullable): a place to store error, or %NULL
 *
 * Call the prepare_statement() in the worker thread
 */
GObject *
_gda_server_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
					GdaStatement *stmt, GdaSet *params,
					GdaStatementModelUsage model_usage,
					GType *col_types, GdaSet **last_inserted_row, GError **error)
{
	if (last_inserted_row)
		*last_inserted_row = NULL;

	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}
	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerExecuteStatementData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.stmt = stmt;
	data.params = params;
	data.model_usage = model_usage;
	data.col_types = col_types;
	data.last_inserted_row = last_inserted_row;

	gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_statement_execute, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage (cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval;
}

/***********************************************************************************************************/

/*
 *   JOB_STMT_TO_SQL
 *   WorkerStmtToSQLData
 *
 */
typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
	GdaStatement          *stmt;
	GdaSet                *params;
	GdaStatementSqlFlag    flags;
	GSList               **params_used;
} WorkerStmtToSQLData;

static gpointer
worker_stmt_to_sql (WorkerStmtToSQLData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	if (fset->statement_to_sql)
		return fset->statement_to_sql (data->provider, data->cnc, data->stmt, data->params, data->flags,
					       data->params_used, error);
	else
		return NULL;
}

gchar *
_gda_server_provider_statement_to_sql  (GdaServerProvider *provider, GdaConnection *cnc,
					GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
					GSList **params_used, GError **error)
{
	GdaWorker *worker;
	if (params_used)
		*params_used = NULL;

	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (!params || GDA_IS_SET (params), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerStmtToSQLData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.stmt = stmt;
	data.params = params;
	data.flags = flags;
	data.params_used = params_used;

	if (cnc)
		gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_stmt_to_sql, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	if (cnc) {
		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
	}

	gda_worker_unref (worker);

	return (gchar *) retval;
}

/***********************************************************************************************************/

/*
 *   JOB_IDENTIFIER_QUOTE
 *   WorkerIdentifierQuoteData
 *
 */
typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
	const gchar           *id;
	gboolean               for_meta_store;
	gboolean               force_quotes;
} WorkerIdentifierQuoteData;

static gpointer
worker_identifier_quote (WorkerIdentifierQuoteData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	if (fset->identifier_quote)
		return fset->identifier_quote (data->provider, data->cnc, data->id, data->for_meta_store, data->force_quotes);
	else
		return NULL;
}

gchar *
_gda_server_provider_identifier_quote (GdaServerProvider *provider, GdaConnection *cnc,
				       const gchar *id, gboolean for_meta_store, gboolean force_quotes)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
		g_return_val_if_fail (gda_connection_is_opened (cnc), NULL);

		gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

		GdaServerProviderConnectionData *cdata;
		cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
		if (!cdata) {
			gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
			g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
			return FALSE;
		}
		worker = gda_worker_ref (cdata->worker);
	}
	else
		worker = _gda_server_provider_create_worker (provider, FALSE);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerIdentifierQuoteData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.id = id;
	data.for_meta_store = for_meta_store;
	data.force_quotes = force_quotes;

	if (cnc)
		gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_identifier_quote, (gpointer) &data, NULL, NULL, NULL);
	if (context)
		g_main_context_unref (context);

	if (cnc) {
		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
	}

	gda_worker_unref (worker);

	return (gchar *) retval;
}

/***********************************************************************************************************/

/*
 *   JOB_META
 *   WorkerMetaData
 *
 */
typedef struct {
	GdaWorker                *worker;
	GdaServerProvider        *provider;
	GdaConnection            *cnc;
	GdaMetaStore             *meta;
	GdaMetaContext           *ctx;
	GdaServerProviderMetaType type;
	guint                     nargs;
	const GValue             *values[5]; /* 5 at most */
} WorkerMetaData;

typedef gboolean (*Meta0Func) (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **);
typedef gboolean (*Meta1Func) (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **, const GValue *);
typedef gboolean (*Meta2Func) (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **, const GValue *, const GValue *);
typedef gboolean (*Meta3Func) (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **, const GValue *, const GValue *, const GValue *);
typedef gboolean (*Meta4Func) (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **, const GValue *, const GValue *, const GValue *, const GValue *);
typedef gboolean (*Meta5Func) (GdaServerProvider *, GdaConnection *, GdaMetaStore *, GdaMetaContext *, GError **, const GValue *, const GValue *, const GValue *, const GValue *, const GValue *);


static gpointer
worker_meta (WorkerMetaData *data, GError **error)
{
	gpointer *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_META);

	gboolean retval;
	switch (data->nargs) {
	case 0: {/* function with no argument */
		Meta0Func func;
		func = (Meta0Func) fset [data->type];
		if (func)
			retval = func (data->provider, data->cnc, data->meta, data->ctx, error);
		else {
			retval = FALSE;
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Not supported"));
		}
		break;
	}
	case 1: {/* function with 1 argument */
		Meta1Func func;
		func = (Meta1Func) fset [data->type];
		if (func)
			retval = func (data->provider, data->cnc, data->meta, data->ctx, error, data->values [0]);
		else {
			retval = FALSE;
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Not supported"));
		}
		break;
	}
	case 2: {/* function with 2 arguments */
		Meta2Func func;
		func = (Meta2Func) fset [data->type];
		if (func)
			retval = func (data->provider, data->cnc, data->meta, data->ctx, error, data->values [0], data->values [1]);
		else {
			retval = FALSE;
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Not supported"));
		}
		break;
	}
	case 3: {/* function with 3 arguments */
		Meta3Func func;
		func = (Meta3Func) fset [data->type];
		if (func)
			retval = func (data->provider, data->cnc, data->meta, data->ctx, error, data->values [0], data->values [1], data->values [2]);
		else {
			retval = FALSE;
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Not supported"));
		}
		break;
	}
	case 4: {/* function with 4 arguments */
		Meta4Func func;
		func = (Meta4Func) fset [data->type];
		if (func)
			retval = func (data->provider, data->cnc, data->meta, data->ctx, error, data->values [0], data->values [1], data->values [2], data->values [3]);
		else {
			retval = FALSE;
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Not supported"));
		}
		break;
	}
	case 5: {/* function with 5 arguments */
		Meta5Func func;
		func = (Meta5Func) fset [data->type];
		if (func)
			retval = func (data->provider, data->cnc, data->meta, data->ctx, error, data->values [0], data->values [1], data->values [2], data->values [3], data->values [4]);
		else {
			retval = FALSE;
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Not supported"));
		}
		break;
	}
	default:
		g_assert_not_reached ();
	}

	return retval ? (gpointer) 0x01 : NULL;
}

static guint
get_meta_nb_values_args (GdaServerProviderMetaType type)
{
	switch (type) {
	case GDA_SERVER_META__INFO:
	case GDA_SERVER_META__BTYPES:
	case GDA_SERVER_META__UDT:
	case GDA_SERVER_META__UDT_COLS:
	case GDA_SERVER_META__ENUMS:
	case GDA_SERVER_META__DOMAINS:
	case GDA_SERVER_META__CONSTRAINTS_DOM:
	case GDA_SERVER_META__EL_TYPES:
	case GDA_SERVER_META__COLLATIONS:
	case GDA_SERVER_META__CHARACTER_SETS:
	case GDA_SERVER_META__SCHEMATA:
	case GDA_SERVER_META__TABLES_VIEWS:
	case GDA_SERVER_META__COLUMNS:
	case GDA_SERVER_META__VIEW_COLS:
	case GDA_SERVER_META__CONSTRAINTS_TAB:
	case GDA_SERVER_META__CONSTRAINTS_REF:
	case GDA_SERVER_META__KEY_COLUMNS:
	case GDA_SERVER_META__CHECK_COLUMNS:
	case GDA_SERVER_META__TRIGGERS:
	case GDA_SERVER_META__ROUTINES:
	case GDA_SERVER_META__ROUTINE_COL:
	case GDA_SERVER_META__ROUTINE_PAR:
	case GDA_SERVER_META__INDEXES_TAB:
	case GDA_SERVER_META__INDEX_COLS:
		return 0;

	case GDA_SERVER_META_EL_TYPES:
		return 1;

	case GDA_SERVER_META_UDT:
	case GDA_SERVER_META_DOMAINS:
	case GDA_SERVER_META_SCHEMATA:
		return 2;

	case GDA_SERVER_META_UDT_COLS:
	case GDA_SERVER_META_ENUMS:
	case GDA_SERVER_META_CONSTRAINTS_DOM:
	case GDA_SERVER_META_COLLATIONS:
	case GDA_SERVER_META_CHARACTER_SETS:
	case GDA_SERVER_META_TABLES_VIEWS:
	case GDA_SERVER_META_COLUMNS:
	case GDA_SERVER_META_VIEW_COLS:
	case GDA_SERVER_META_TRIGGERS:
	case GDA_SERVER_META_ROUTINES:
	case GDA_SERVER_META_ROUTINE_PAR:
		return 3;

	case GDA_SERVER_META_CONSTRAINTS_TAB:
	case GDA_SERVER_META_CONSTRAINTS_REF:
	case GDA_SERVER_META_KEY_COLUMNS:
	case GDA_SERVER_META_CHECK_COLUMNS:
	case GDA_SERVER_META_INDEXES_TAB:
	case GDA_SERVER_META_INDEX_COLS:
		return 4;

	case GDA_SERVER_META_ROUTINE_COL:
		return 5;
	
	default:
		g_assert_not_reached ();
	}
}

/*
 * @call_error: (nullable)
 * @loc_error: (nullable)
 */
static gboolean
meta_finalize_result (gpointer retval, GError **call_error, GError **loc_error)
{
	if (retval) {
		g_clear_error (loc_error);
		return TRUE;
	}
	if (loc_error && *loc_error && (*loc_error)->message) {
		g_set_error (call_error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     _("Internal error please report bug to https://gitlab.gnome.org/GNOME/libgda/issues Reported error is: %s"),
			     (*loc_error)->message);
  } else {
		g_set_error (call_error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     _("Internal error please report bug to https://gitlab.gnome.org/GNOME/libgda/issues No error was reported"));
	}
	g_clear_error (loc_error);
	return FALSE;
}

gboolean
_gda_server_provider_meta_0arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, GError **error)
{
	gpointer retval = NULL;
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	/* check that function at index @type has 0 value argument */
	if (get_meta_nb_values_args (type) != 0) {
		g_warning ("Internal error: function %s() is only for meta data with no value argument", __FUNCTION__);
		goto out;
	}
	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		goto out;
	}

	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerMetaData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.meta = meta;
	data.ctx = ctx;
	data.type = type;
	data.nargs = 0;
	data.values[0] = NULL;
	data.values[1] = NULL;
	data.values[2] = NULL;
	data.values[3] = NULL;

	GError *lerror = NULL;
	if (cnc)
		gda_connection_increase_usage (cnc); /* USAGE ++ */
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_meta, (gpointer) &data, NULL, NULL, &lerror);
	if (context)
		g_main_context_unref (context);

	if (cnc) {
		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
	}

	gda_worker_unref (worker);

 out:
	return meta_finalize_result (retval, error, &lerror);
}

gboolean
_gda_server_provider_meta_1arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, const GValue *value0, GError **error)
{
	gpointer retval = NULL;
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	/* check that function at index @type has 1 value argument */
	if (get_meta_nb_values_args (type) != 1) {
		g_warning ("Internal error: function %s() is only for meta data with 1 value argument", __FUNCTION__);
		goto out;
	}
	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		goto out;
	}

	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerMetaData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.meta = meta;
	data.ctx = ctx;
	data.type = type;
	data.nargs = 1;
	data.values[0] = value0;
	data.values[1] = NULL;
	data.values[2] = NULL;
	data.values[3] = NULL;

	GError *lerror = NULL;
	if (cnc)
		gda_connection_increase_usage (cnc); /* USAGE ++ */
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_meta, (gpointer) &data, NULL, NULL, &lerror);
	if (context)
		g_main_context_unref (context);

	if (cnc) {
		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
	}

	gda_worker_unref (worker);

 out:
	return meta_finalize_result (retval, error, &lerror);
}

gboolean
_gda_server_provider_meta_2arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, const GValue *value0, const GValue *value1, GError **error)
{
	gpointer retval = NULL;
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	/* check that function at index @type has 2 values arguments */
	if (get_meta_nb_values_args (type) != 2) {
		g_warning ("Internal error: function %s() is only for meta data with 2 values arguments", __FUNCTION__);
		goto out;
	}
	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		goto out;
	}

	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerMetaData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.meta = meta;
	data.ctx = ctx;
	data.type = type;
	data.nargs = 2;
	data.values[0] = value0;
	data.values[1] = value1;
	data.values[2] = NULL;
	data.values[3] = NULL;

	GError *lerror = NULL;
	if (cnc)
		gda_connection_increase_usage (cnc); /* USAGE -- */
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_meta, (gpointer) &data, NULL, NULL, &lerror);
	if (context)
		g_main_context_unref (context);

	if (cnc) {
		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
	}

	gda_worker_unref (worker);

 out:
	return meta_finalize_result (retval, error, &lerror);
}

gboolean
_gda_server_provider_meta_3arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, const GValue *value0, const GValue *value1,
				const GValue *value2, GError **error)
{
	gpointer retval = NULL;
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	/* check that function at index @type has 3 values arguments */
	if (get_meta_nb_values_args (type) != 3) {
		g_warning ("Internal error: function %s() is only for meta data with 3 values arguments", __FUNCTION__);
		goto out;
	}
	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		goto out;
	}

	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerMetaData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.meta = meta;
	data.ctx = ctx;
	data.type = type;
	data.nargs = 3;
	data.values[0] = value0;
	data.values[1] = value1;
	data.values[2] = value2;
	data.values[3] = NULL;

	GError *lerror = NULL;
	if (cnc)
		gda_connection_increase_usage (cnc); /* USAGE ++ */
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_meta, (gpointer) &data, NULL, NULL, &lerror);
	if (context)
		g_main_context_unref (context);

	if (cnc) {
		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
	}

	gda_worker_unref (worker);
 out:
	return meta_finalize_result (retval, error, &lerror);
}

gboolean
_gda_server_provider_meta_4arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, const GValue *value0, const GValue *value1,
				const GValue *value2, const GValue *value3, GError **error)
{
	gpointer retval = NULL;
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	/* check that function at index @type has 4 values arguments */
	if (get_meta_nb_values_args (type) != 4) {
		g_warning ("Internal error: function %s() is only for meta data with 4 values arguments", __FUNCTION__);
		goto out;
	}
	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		goto out;
	}

	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerMetaData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.meta = meta;
	data.ctx = ctx;
	data.type = type;
	data.nargs = 4;
	data.values[0] = value0;
	data.values[1] = value1;
	data.values[2] = value2;
	data.values[3] = value3;

	GError *lerror = NULL;
	if (cnc)
		gda_connection_increase_usage (cnc); /* USAGE ++ */
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_meta, (gpointer) &data, NULL, NULL, &lerror);
	if (context)
		g_main_context_unref (context);

	if (cnc) {
		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
	}

	gda_worker_unref (worker);

 out:
	return meta_finalize_result (retval, error, &lerror);
}


gboolean
_gda_server_provider_meta_5arg (GdaServerProvider *provider, GdaConnection *cnc,
				GdaMetaStore *meta, GdaMetaContext *ctx,
				GdaServerProviderMetaType type, const GValue *value0, const GValue *value1,
				const GValue *value2, const GValue *value3, const GValue *value4, GError **error)
{
	gpointer retval = NULL;
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	/* check that function at index @type has 5 values arguments */
	if (get_meta_nb_values_args (type) != 5) {
		g_warning ("Internal error: function %s() is only for meta data with 5 values arguments", __FUNCTION__);
		goto out;
	}
	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		goto out;
	}

	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerMetaData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.meta = meta;
	data.ctx = ctx;
	data.type = type;
	data.nargs = 4;
	data.values[0] = value0;
	data.values[1] = value1;
	data.values[2] = value2;
	data.values[3] = value3;
	data.values[4] = value4;

	GError *lerror = NULL;
	if (cnc)
		gda_connection_increase_usage (cnc); /* USAGE ++ */
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_meta, (gpointer) &data, NULL, NULL, &lerror);
	if (context)
		g_main_context_unref (context);

	if (cnc) {
		gda_connection_decrease_usage (cnc); /* USAGE -- */
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
	}

	gda_worker_unref (worker);

 out:
	return meta_finalize_result (retval, error, &lerror);
}

/***********************************************************************************************************/

/*
 *   JOB_BEGIN_TRANSACTION
 *   JOB_COMMIT_TRANSACTION
 *   JOB_ROLLBACK_TRANSACTION
 *   WorkerTransactionData
 */
typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
	const gchar           *name;
	GdaTransactionIsolation level;
} WorkerTransactionData;

static gpointer
worker_begin_transaction (WorkerTransactionData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	guint delay;
	delay = _gda_connection_get_exec_slowdown (data->cnc);
	if (delay > 0) {
		g_print (_("Delaying transaction for %u ms\n"), delay / 1000);
		g_usleep (delay);
	}

	gboolean retval;
	if (fset->begin_transaction)
		retval = fset->begin_transaction (data->provider, data->cnc, data->name, data->level, error);
	else {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Database provider does not support transactions"));
		retval = FALSE;
	}
	return retval ? (gpointer) 0x01: NULL;
}

gboolean
_gda_server_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GdaTransactionIsolation level, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, FALSE);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}
	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerTransactionData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.name = name;
	data.level = level;

	gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_begin_transaction, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage (cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

static gpointer
worker_commit_transaction (WorkerTransactionData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	guint delay;
	delay = _gda_connection_get_exec_slowdown (data->cnc);
	if (delay > 0) {
		g_print (_("Delaying transaction for %u ms\n"), delay / 1000);
		g_usleep (delay);
	}

	gboolean retval;
	if (fset->commit_transaction)
		retval = fset->commit_transaction (data->provider, data->cnc, data->name, error);
	else {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Database provider does not support transactions"));
		retval = FALSE;
	}
	return retval ? (gpointer) 0x01: NULL;
}

gboolean
_gda_server_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					 const gchar *name, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, FALSE);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}
	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerTransactionData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.name = name;

	gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_commit_transaction, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage (cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

static gpointer
worker_rollback_transaction (WorkerTransactionData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	guint delay;
	delay = _gda_connection_get_exec_slowdown (data->cnc);
	if (delay > 0) {
		g_print (_("Delaying transaction for %u ms\n"), delay / 1000);
		g_usleep (delay);
	}

	gboolean retval;
	if (fset->rollback_transaction)
		retval = fset->rollback_transaction (data->provider, data->cnc, data->name, error);
	else {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Database provider does not support transactions"));
		retval = FALSE;
	}
	return retval ? (gpointer) 0x01: NULL;
}

gboolean
_gda_server_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					   const gchar *name, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, FALSE);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}
	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerTransactionData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.name = name;

	gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_rollback_transaction, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage (cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

static gpointer
worker_add_savepoint (WorkerTransactionData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	gboolean retval;
	if (fset->add_savepoint)
		retval = fset->add_savepoint (data->provider, data->cnc, data->name, error);
	else {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Database provider does not support savepoints"));
		retval = FALSE;
	}
	return retval ? (gpointer) 0x01: NULL;
}

gboolean
_gda_server_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc, const gchar *name, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, FALSE);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}
	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerTransactionData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.name = name;

	gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_add_savepoint, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage (cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

static gpointer
worker_rollback_savepoint (WorkerTransactionData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	gboolean retval;
	if (fset->rollback_savepoint)
		retval = fset->rollback_savepoint (data->provider, data->cnc, data->name, error);
	else {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Database provider does not support savepoints"));
		retval = FALSE;
	}
	return retval ? (gpointer) 0x01: NULL;
}

gboolean
_gda_server_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc, const gchar *name, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, FALSE);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}
	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerTransactionData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.name = name;

	gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_rollback_savepoint, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage (cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

static gpointer
worker_delete_savepoint (WorkerTransactionData *data, GError **error)
{
	GdaServerProviderBase *fset;
	fset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_BASE);

	gboolean retval;
	if (fset->delete_savepoint)
		retval = fset->delete_savepoint (data->provider, data->cnc, data->name, error);
	else {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
			     "%s", _("Database provider does not support savepoints"));
		retval = FALSE;
	}
	return retval ? (gpointer) 0x01: NULL;
}

gboolean
_gda_server_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc, const gchar *name, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, FALSE);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}
	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerTransactionData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.name = name;

	gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_delete_savepoint, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage (cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

/***********************************************************************************************************/

/*
 * JOB_XA_START
 * JOB_XA_END
 * JOB_XA_PREPARE
 * JOB_XA_COMMIT
 * JOB_XA_ROLLBACK
 * JOB_XA_RECOVER
 */

typedef struct {
	GdaWorker             *worker;
	GdaServerProvider     *provider;
	GdaConnection         *cnc;
	const GdaXaTransactionId *trx;
	GdaXaType              type;
} WorkerXAData;

static gpointer
worker_xa (WorkerXAData *data, GError **error)
{
	GdaServerProviderXa *xaset;
	xaset = _gda_server_provider_get_impl_functions (data->provider, data->worker, GDA_SERVER_PROVIDER_FUNCTIONS_XA);

	guint delay;
	delay = _gda_connection_get_exec_slowdown (data->cnc);
	if (delay > 0) {
		g_print (_("Delaying distributed transaction for %u ms\n"), delay / 1000);
		g_usleep (delay);
	}

	switch (data->type) {
	case GDA_XA_START: {
		gboolean retval;
		if (xaset->xa_start)
			retval = xaset->xa_start (data->provider, data->cnc, data->trx, error);
		else {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Database provider does not support distributed transactions"));
			retval = FALSE;
		}
		return retval ? (gpointer) 0x01: NULL;
	}
	case GDA_XA_END: {
		gboolean retval;
		if (xaset->xa_end)
			retval = xaset->xa_end (data->provider, data->cnc, data->trx, error);
		else {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Database provider does not support distributed transactions"));
			retval = FALSE;
		}
		return retval ? (gpointer) 0x01: NULL;
	}
	case GDA_XA_PREPARE: {
		gboolean retval;
		if (xaset->xa_prepare)
			retval = xaset->xa_prepare (data->provider, data->cnc, data->trx, error);
		else {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Database provider does not support distributed transactions"));
			retval = FALSE;
		}
		return retval ? (gpointer) 0x01: NULL;
	}
	case GDA_XA_COMMIT: {
		gboolean retval;
		if (xaset->xa_commit)
			retval = xaset->xa_commit (data->provider, data->cnc, data->trx, error);
		else {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Database provider does not support distributed transactions"));
			retval = FALSE;
		}
		return retval ? (gpointer) 0x01: NULL;
	}
	case GDA_XA_ROLLBACK: {
		gboolean retval;
		if (xaset->xa_rollback)
			retval = xaset->xa_rollback (data->provider, data->cnc, data->trx, error);
		else {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Database provider does not support distributed transactions"));
			retval = FALSE;
		}
		return retval ? (gpointer) 0x01: NULL;
	}
	case GDA_XA_RECOVER: {
		GList *retval;
		if (xaset->xa_recover)
			retval = xaset->xa_recover (data->provider, data->cnc, error);
		else {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_METHOD_NON_IMPLEMENTED_ERROR,
				     "%s", _("Database provider does not support distributed transactions"));
			retval = NULL;
		}
		return retval;
	}
	default:
		g_assert_not_reached ();
	}
	return NULL; /* never reached */
}

gboolean
_gda_server_provider_xa (GdaServerProvider *provider, GdaConnection *cnc, const GdaXaTransactionId *trx,
			 GdaXaType type, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, FALSE);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}
	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerXAData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.trx = trx;
	data.type = type;

	gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_xa, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage (cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return retval ? TRUE : FALSE;
}

GList *
_gda_server_provider_xa_recover (GdaServerProvider *provider, GdaConnection *cnc, GError **error)
{
	GdaWorker *worker;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	gda_lockable_lock ((GdaLockable*) cnc); /* CNC LOCK */

	GdaServerProviderConnectionData *cdata;
	cdata = gda_connection_internal_get_provider_data_error (cnc, FALSE);
	if (!cdata) {
		gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */
		g_warning ("Internal error: connection reported as opened, yet no provider's data has been setted");
		return FALSE;
	}
	worker = gda_worker_ref (cdata->worker);

	GMainContext *context;
	context = gda_server_provider_get_real_main_context (cnc);

	WorkerXAData data;
	data.worker = worker;
	data.provider = provider;
	data.cnc = cnc;
	data.trx = NULL;
	data.type = GDA_XA_RECOVER;

	gda_connection_increase_usage (cnc); /* USAGE ++ */
	gpointer retval;
	gda_worker_do_job (worker, context, 0, &retval, NULL,
			   (GdaWorkerFunc) worker_xa, (gpointer) &data, NULL, NULL, error);
	if (context)
		g_main_context_unref (context);

	gda_connection_decrease_usage (cnc); /* USAGE -- */
	gda_lockable_unlock ((GdaLockable*) cnc); /* CNC UNLOCK */

	gda_worker_unref (worker);

	return (GList*) retval;
}

/***********************************************************************************************************/

/*
 * server_provider_job_done_callback:
 *
 * Generic function called whenever a job submitted by a connection's internal GdaWorker has finished,
 * with the exception of jobs submitted using gda_worker_do_job()
 */
static void
server_provider_job_done_callback (GdaWorker *worker, guint job_id, gpointer result, GError *error, GdaServerProvider *provider)
{
  GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (provider);
	WorkerData *wd = NULL;
	if (priv->jobs_hash)
		wd = g_hash_table_lookup (priv->jobs_hash, &job_id);
	g_assert (wd);

	switch (wd->job_type) {
	case JOB_OPEN_CONNECTION: {
		WorkerOpenConnectionData *sdata = (WorkerOpenConnectionData*) wd->job_data;
		sdata->callback (sdata->cnc, wd->job_id,
				 stage2_open_connection (worker, sdata->cnc, result),
				 error, sdata->callback_data);
		break;
	}
	default:
		/* should not be reached because there is no ASYNC version of the close connection call */
		g_assert_not_reached ();
	}

	g_hash_table_remove (priv->jobs_hash, &job_id); /* will call worker_data_free() */
}

/* Code from gda-server-provider-extra.c */

/**
 * gda_server_provider_internal_get_parser:
 * @prov: a #GdaServerProvider
 *
 * This is a factory method to get a unique instance of a #GdaSqlParser object
 * for each #GdaServerProvider object
 * Don't unref it.
 *
 * Returns: (transfer none): a #GdaSqlParser
 */
GdaSqlParser *
gda_server_provider_internal_get_parser (GdaServerProvider *prov)
{
	GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (prov);
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (prov), NULL);
	if (priv->parser)
		return priv->parser;
	priv->parser = gda_server_provider_create_parser (prov, NULL);
	if (!priv->parser)
		priv->parser = gda_sql_parser_new ();
	return priv->parser;
}

/**
 * gda_server_provider_perform_operation_default:
 * @provider: a #GdaServerProvider object
 * @cnc: (nullable): a #GdaConnection object which will be used to perform an action, or %NULL
 * @op: a #GdaServerOperation object
 * @error: a place to store an error, or %NULL
 *
 * Performs the operation described by @op, using the SQL from the rendering of the operation
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_server_provider_perform_operation_default (GdaServerProvider *provider, GdaConnection *cnc,
					       GdaServerOperation *op, GError **error)
{
	gchar *sql;
	GdaBatch *batch;
	const GSList *list;
	gboolean retval = TRUE;

	sql = gda_server_provider_render_operation (provider, cnc, op, error);
	if (!sql)
		return FALSE;

	GdaSqlParser *parser;
	parser = gda_server_provider_internal_get_parser (provider); /* no ref held! */
	batch = gda_sql_parser_parse_string_as_batch (parser, sql, NULL, error);
	g_free (sql);
	if (!batch)
		return FALSE;

	for (list = gda_batch_get_statements (batch); list; list = list->next) {
		if (gda_connection_statement_execute_non_select (cnc, GDA_STATEMENT (list->data), NULL, NULL, error) == -1) {
			retval = FALSE;
			break;
		}
	}
	g_object_unref (batch);

	return retval;
}

/**
 * gda_server_provider_handler_use_default:
 * @provider: a server provider
 * @type: a #GType
 *
 * Reserved to database provider's implementations. This method defines a default data handler for
 * @provider, and returns that #GdaDataHandler.
 *
 * Returns: (transfer none): a #GdaDataHandler, or %NULL
 *
 * Since: 5.2
 */
GdaDataHandler *
gda_server_provider_handler_use_default (GdaServerProvider *provider, GType type)
{
	GdaDataHandler *dh = NULL;
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (provider), NULL);
	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_UINT64) ||
	    (type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == GDA_TYPE_USHORT) ||
	    (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) ||
	    (type == G_TYPE_UINT) ||
	    (type == G_TYPE_LONG) ||
	    (type == G_TYPE_ULONG)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_numerical_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT64, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UINT64, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DOUBLE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_NUMERIC, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_FLOAT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_SHORT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_USHORT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_CHAR, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UCHAR, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UINT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_ULONG, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_LONG, NULL);
			g_object_unref (dh);
		}
	}
        else if ((type == GDA_TYPE_BINARY) ||
		 (type == GDA_TYPE_BLOB)) {
		/* no default binary data handler, it's too database specific */
		dh = NULL;
	}
        else if (type == G_TYPE_BOOLEAN) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_boolean_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_BOOLEAN, NULL);
			g_object_unref (dh);
		}
	}
	else if ((type == GDA_TYPE_TIME) ||
		 (type == G_TYPE_DATE_TIME) ||
		 (type == G_TYPE_DATE)) {
		/* no default time related data handler, it's too database specific */
		dh = NULL;
	}
	else if (type == G_TYPE_STRING) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_string_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_STRING, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == GDA_TYPE_TEXT) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_text_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TEXT, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == G_TYPE_GTYPE) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_type_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_GTYPE, NULL);
			g_object_unref (dh);
		}
	}

	return dh;
}

static gboolean
param_to_null_foreach (GdaSqlAnyPart *part, G_GNUC_UNUSED gpointer data, G_GNUC_UNUSED GError **error)
{
	if (part->type == GDA_SQL_ANY_EXPR) {
		GdaSqlExpr *expr = (GdaSqlExpr*) part;
		if (expr->param_spec) {
			GType type = expr->param_spec->g_type;
			gda_sql_param_spec_free (expr->param_spec);
			expr->param_spec = NULL;

			if (!expr->value) {
				if (type != GDA_TYPE_NULL)
					expr->value = gda_value_new_from_string ("0", type);
				else
					g_value_set_int ((expr->value = gda_value_new (G_TYPE_INT)), 0);
			}
		}
	}
	return TRUE;
}

/**
 * gda_select_alter_select_for_empty:
 * @stmt: a SELECT #GdaStatement
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Creates a new #GdaStatement, selecting the same data as @stmt, but which always returns an
 * empty (no row) data model. This is use dy database providers' implementations.
 *
 * Returns: (transfer full): a new #GdaStatement
 */
GdaStatement *
gda_select_alter_select_for_empty (GdaStatement *stmt, G_GNUC_UNUSED GError **error)
{
	GdaStatement *estmt;
	GdaSqlStatement *sqlst;
	GdaSqlStatementSelect *stsel;

	g_assert (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT);
	g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
	g_assert (sqlst);

	if (sqlst->sql) {
		g_free (sqlst->sql);
		sqlst->sql = NULL;
	}
	stsel = (GdaSqlStatementSelect*) sqlst->contents;

	/* set the WHERE condition to "1 = 0" */
	GdaSqlExpr *expr, *cond = stsel->where_cond;
	GdaSqlOperation *op;
	if (cond)
		gda_sql_expr_free (cond);
	cond = gda_sql_expr_new (GDA_SQL_ANY_PART (stsel));
	stsel->where_cond = cond;
	op = gda_sql_operation_new (GDA_SQL_ANY_PART (cond));
	cond->cond = op;
	op->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;
	expr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
	op->operands = g_slist_prepend (NULL, expr);
	g_value_set_int ((expr->value = gda_value_new (G_TYPE_INT)), 1);
	expr = gda_sql_expr_new (GDA_SQL_ANY_PART (op));
	op->operands = g_slist_prepend (op->operands, expr);
	g_value_set_int ((expr->value = gda_value_new (G_TYPE_INT)), 0);

	/* replace any selected field which has a parameter with NULL */
	gda_sql_any_part_foreach (GDA_SQL_ANY_PART (stsel), (GdaSqlForeachFunc) param_to_null_foreach,
				  NULL, NULL);

	/* create new statement */
	estmt = g_object_new (GDA_TYPE_STATEMENT, "structure", sqlst, NULL);
	gda_sql_statement_free (sqlst);
	return estmt;
}

/**
 * gda_server_provider_handler_find:
 * @prov: a #GdaServerProvider
 * @cnc: (nullable): a #GdaConnection
 * @g_type: a #GType
 * @dbms_type: (nullable): a database type
 *
 * Reserved to database provider's implementations: get the #GdaDataHandler associated to @prov
 * for connection @cnc. You probably want to use gda_server_provider_get_data_handler_g_type().
 *
 * Returns: (transfer none): the requested #GdaDataHandler, or %NULL if none found
 */
GdaDataHandler *
gda_server_provider_handler_find (GdaServerProvider *prov, GdaConnection *cnc,
				  GType g_type, const gchar *dbms_type)
{
	g_return_val_if_fail (GDA_IS_SERVER_PROVIDER (prov), NULL);
	GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (prov);
	if (cnc)
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	GdaDataHandler *dh;
	GdaServerProviderHandlerInfo info;

	info.cnc = cnc;
	info.g_type = g_type;
	info.dbms_type = (gchar *) dbms_type;
	dh = g_hash_table_lookup (priv->data_handlers, &info);
	if (!dh) {
		/* try without the connection specification */
		info.cnc = NULL;
		dh = g_hash_table_lookup (priv->data_handlers, &info);
	}

	return dh;
}

void
gda_server_provider_handler_declare (GdaServerProvider *prov, GdaDataHandler *dh,
				     GdaConnection *cnc,
				     GType g_type, const gchar *dbms_type)
{
	GdaServerProviderHandlerInfo *info;
	g_return_if_fail (GDA_IS_SERVER_PROVIDER (prov));
	g_return_if_fail (GDA_IS_DATA_HANDLER (dh));
	GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (prov);

	info = g_new (GdaServerProviderHandlerInfo, 1);
	info->cnc = cnc;
	info->g_type = g_type;
	info->dbms_type = dbms_type ? g_strdup (dbms_type) : NULL;

	g_hash_table_insert (priv->data_handlers, info, dh);
	g_object_ref (dh);
}

static gboolean
handlers_clear_for_cnc_fh (GdaServerProviderHandlerInfo *key, GdaDataHandler *value, GdaConnection *cnc)
{
	return (key->cnc == cnc) ? TRUE : FALSE;
}

/*
 * Removes any #GdaServerProviderHandlerInfo associated to @cnc */
void
_gda_server_provider_handlers_clear_for_cnc (GdaServerProvider *prov, GdaConnection *cnc)
{
	g_return_if_fail (GDA_IS_SERVER_PROVIDER (prov));
	g_return_if_fail (GDA_IS_CONNECTION (cnc));
	GdaServerProviderPrivate *priv = gda_server_provider_get_instance_private (prov);
	g_hash_table_foreach_remove (priv->data_handlers, (GHRFunc) handlers_clear_for_cnc_fh, cnc);
}

/**
 * gda_server_provider_load_resource_contents:
 * @prov_name: the provider's name
 * @resource: the name of the resource to load
 *
 * Loads and returns the contents of the specified resource.
 * This function should only be used by database provider's implementations
 *
 * Returns: (transfer full): a new string containing the resource's contents, or %NULL if not found or if an error occurred
 *
 * Since: 6.0
 */
gchar *
gda_server_provider_load_resource_contents (const gchar *prov_name, const gchar *resource)
{
	g_return_val_if_fail (prov_name, NULL);
	g_return_val_if_fail (resource, NULL);

	gchar *rname;
	rname = g_strdup_printf ("/spec/%s/%s", prov_name, resource);

	GBytes *bytes;
	bytes = g_resources_lookup_data (rname, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	g_free (rname);
	if (!bytes) {
		g_warning ("Resource: /spec/%s/%s, not found for provider", prov_name, resource);
		return NULL;
	}

	gchar *retval;
	retval = g_strdup ((const gchar*) g_bytes_get_data (bytes, NULL));
	g_bytes_unref (bytes);
	return retval;
}
