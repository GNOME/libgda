#include <string.h>
#include <glib.h>

#include "mem.h"

char *
memsql_strappend (const char *string1, const char *string2)
{
	int len = 0;
	char *retval;

	if (!string1 && !string2)
		return NULL;

	if (string1)
		len += strlen (string1);
	if (string2)
		len += strlen (string2);
	retval = g_malloc (len + 1);
	retval[0] = '\0';

	if (string1)
		strcpy (retval, string1);
	if (string2)
		strcat (retval, string2);

	retval[len] = '\0';

	return retval;
}

char *
memsql_strappend_free (char *str1, char *str2)
{
	char *retval;

	retval = memsql_strappend (str1, str2);

	g_free  (str1);
	g_free  (str2);

	return retval;
}
