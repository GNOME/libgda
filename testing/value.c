/* GDA - Test suite
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gda-test.h"

/* Stress test a GdaValue */
static void
stress_value (const gchar *type_str, GdaValue *value)
{
	gchar *str;
	GdaValue *copy;

	g_return_if_fail (type_str != NULL);
	g_return_if_fail (value != NULL);

	g_print ("Testing value of type %s\n", type_str);

	g_print ("\tNative value = ");
	if (gda_value_isa (value, GDA_VALUE_TYPE_BIGINT))
		g_print ("%ld\n", gda_value_get_bigint (value));
	else if (gda_value_isa (value, GDA_VALUE_TYPE_BOOLEAN))
		g_print ("%d\n", gda_value_get_boolean (value));
	else if (gda_value_isa (value, GDA_VALUE_TYPE_DOUBLE))
		g_print ("%f\n", gda_value_get_double (value));
	else if (gda_value_isa (value, GDA_VALUE_TYPE_STRING))
		g_print ("%s\n", gda_value_get_string (value));

	str = gda_value_stringify (value);
	g_print ("\tStringified value = %s\n", str);
	g_free (str);

	copy = gda_value_copy (value);
	str = gda_value_stringify (copy);
	g_print ("\tStringified value after copy = %s\n", str);
	g_free (str);

	gda_value_free (copy);
	gda_value_free (value);
}

/* Main entry point for the values test suite */
void
test_values (void)
{
	DISPLAY_MESSAGE ("Testing GdaValue's");

	stress_value ("bigint", gda_value_new_bigint (123456789L));
	stress_value ("boolean", gda_value_new_boolean (TRUE));
	stress_value ("double", gda_value_new_double (3.1416));
	stress_value ("string", gda_value_new_string ("This is a string"));
}
