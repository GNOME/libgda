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

#include "gda-common-private.h"
#include "gda-corba.h"

/**
 * gda_corba_get_orb
 *
 * Return the CORBA ORB being used by the current program.
 *
 * Returns: a reference to the current CORBA ORB
 */
CORBA_ORB
gda_corba_get_orb (void)
{
	return bonobo_activation_orb_get ();
}

/**
 * gda_corba_get_name_service
 *
 * Return a reference to the CORBA name service.
 *
 * Returns: a reference to the CORBA name service
 */
CORBA_Object
gda_corba_get_name_service (void)
{
	CORBA_Environment ev;
	return bonobo_activation_name_service_get (&ev);
}

/**
 * gda_corba_handle_exception
 * @ev: a CORBA_Environment structure
 *
 * Handles the given exception. Note that this function is very simple: it just
 * logs an error message to the GDA log if there was an exception, and nothing
 * else. So, it is only useful if you just want to know if a CORBA exception
 * happened. If you want more detailed information about the exception,
 * you should handle it yourself.
 *
 * Returns: TRUE if there was no error, FALSE otherwise.
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
gda_corba_get_ac_attribute (CORBA_sequence_Bonobo_ActivationProperty props,
			    const gchar * name)
{
	gchar *ret = NULL;
	gint i, j;

	g_return_val_if_fail (name != NULL, NULL);

	for (i = 0; i < props._length; i++) {
		if (!g_strcasecmp (props._buffer[i].name, name)) {
			switch (props._buffer[i].v._d) {
			case Bonobo_ACTIVATION_P_STRING :
				return g_strdup (props._buffer[i].v._u.
						 value_string);
			case Bonobo_ACTIVATION_P_NUMBER :
				return g_strdup_printf ("%f",
							props._buffer[i].v._u.
							value_number);
			case Bonobo_ACTIVATION_P_BOOLEAN :
				return g_strdup (props._buffer[i].v._u.
						 value_boolean ? _("True") :
						 _("False"));
			case Bonobo_ACTIVATION_P_STRINGV : {
				CORBA_sequence_CORBA_string strlist;
				GString *str = NULL;

				strlist = props._buffer[i].v._u.value_stringv;
				for (j = 0; j < strlist._length; j++) {
					if (!str)
						str = g_string_new (strlist._buffer[j]);
					else {
						str = g_string_append (str, ";");
						str = g_string_append (str,
								       strlist._buffer[j]);
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
 * gda_corba_id_is_active
 * @id: component ID
 */
gboolean
gda_corba_id_is_active (const gchar *id)
{
}
