/* GDA Common Library
 * Copyright (C) 2000 Rodrigo Moya
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

#include "config.h"
#include "gda-corba.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough. */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

/**
 * gda_corba_get_orb
 *
 * Return the CORBA ORB being used by the current program
 */
CORBA_ORB
gda_corba_get_orb (void)
{
	return oaf_orb_get ();
}

/**
 * gda_corba_get_name_service
 *
 * Return a reference to the CORBA name service
 */
CORBA_Object
gda_corba_get_name_service (void)
{
	CORBA_Environment ev;
	return oaf_name_service_get (&ev);
}

/**
 * gda_corba_handle_exception
 *
 */
gboolean
gda_corba_handle_exception (CORBA_Environment * ev)
{
	g_return_val_if_fail (ev != NULL, FALSE);

	switch (ev->_major) {
	case CORBA_NO_EXCEPTION:
		CORBA_exception_free (ev);
		break;
	case CORBA_SYSTEM_EXCEPTION:
		CORBA_exception_free (ev);
		gda_log_error (_("CORBA System exception: %s"),
			       CORBA_exception_id (ev));
		return FALSE;
	case CORBA_USER_EXCEPTION:
		CORBA_exception_free (ev);
		/* FIXME: look at gconf/gconf/gconf-internals.c */
		return FALSE;
	}
	return TRUE;
}

/**
 * gda_corba_get_oaf_attribute
 */
gchar *
gda_corba_get_oaf_attribute (CORBA_sequence_OAF_Property props,
			     const gchar * name)
{
	gchar *ret = NULL;
	gint i, j;

	g_return_val_if_fail (name != NULL, NULL);

	for (i = 0; i < props._length; i++) {
		if (!g_strcasecmp (props._buffer[i].name, name)) {
			switch (props._buffer[i].v._d) {
			case OAF_P_STRING:
				return g_strdup (props._buffer[i].v._u.
						 value_string);
			case OAF_P_NUMBER:
				return g_strdup_printf ("%f",
							props._buffer[i].v._u.
							value_number);
			case OAF_P_BOOLEAN:
				return g_strdup (props._buffer[i].v._u.
						 value_boolean ? _("True") :
						 _("False"));
			case OAF_P_STRINGV:{
					GNOME_stringlist strlist;
					GString *str = NULL;

					strlist =
						props._buffer[i].v._u.
						value_stringv;
					for (j = 0; j < strlist._length; j++) {
						if (!str)
							str = g_string_new
								(strlist.
								 _buffer[j]);
						else {
							str = g_string_append
								(str, ";");
							str = g_string_append
								(str,
								 strlist.
								 _buffer[j]);
						}
					}
					if (str) {
						ret = g_strdup (str->str);
						g_string_free (str, TRUE);
					}
					return ret;
				}
			}
		}
	}
	return ret;
}

/**
 * gda_corba_oafiid_is_active
 */
gboolean
gda_corba_oafiid_is_active (const gchar * oafiid)
{
	OAF_ServerInfoList *servlist;
	CORBA_Environment ev;
	gchar *query;

	g_return_val_if_fail (oafiid != NULL, FALSE);

	query = g_strdup_printf ("iid = '%s' AND _active = 'true'", oafiid);
	CORBA_exception_init (&ev);
	servlist = oaf_query (query, NULL, &ev);
	g_free ((gpointer) query);

	if (gda_corba_handle_exception (&ev)) {
		CORBA_exception_free (&ev);
		/* FIXME: free servlist */
		return servlist->_length == 0 ? FALSE : TRUE;
	}
	return FALSE;
}
