/* GDA common library
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include "gda-binreloc.h"
/* include source file as mentionned in gbr_init_lib()'s doc */
#include "binreloc.c"

#ifdef G_OS_WIN32
#include <windows.h>
/* Remember HMODULE to retrieve path to it lateron */
static HMODULE hdllmodule = NULL;
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		hdllmodule = (HMODULE)hinstDLL;
		break;
	}

	return TRUE;
}
#elif HAVE_CARBON
#include <Carbon/Carbon.h>
#endif


/**
 * gda_gbr_init
 */
void
gda_gbr_init (void)
{
#ifdef G_OS_WIN32
	/* nothing */
#elif HAVE_CARBON
	/* nothing */
#else
	_gda_gbr_init_lib (NULL);
#endif
}

/*
 * gda_gbr_get_file_path
 */
gchar *
gda_gbr_get_file_path (GdaPrefixDir where, ...)
{
	gchar *prefix = NULL;
	gchar *tmp, *file_part;
	va_list ap;
	gchar **parts;
	gint size, i;
	const gchar *prefix_dir_name = NULL;
	gint prefix_len = strlen (LIBGDAPREFIX);

#ifdef G_OS_WIN32
	wchar_t path[MAX_PATH];
	gchar* p;
#endif
	switch (where) {
	default:
	case GDA_NO_DIR:
		break;
	case GDA_BIN_DIR:
		tmp = LIBGDABIN;
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else
			prefix_dir_name = "bin";
		break;
	case GDA_SBIN_DIR:
		tmp = LIBGDASBIN;
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else
			prefix_dir_name = tmp + prefix_len + 1;
		break;
	case GDA_DATA_DIR:
		tmp = LIBGDADATA;
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else
			prefix_dir_name = tmp + prefix_len + 1;
		break;
	case GDA_LOCALE_DIR:
		tmp = LIBGDADATA;
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) {
			prefix = g_strdup (tmp);
			prefix_dir_name = "locale";
		}
		else
			prefix_dir_name = "share" G_DIR_SEPARATOR_S "locale";
		break;
	case GDA_LIB_DIR:
		tmp = LIBGDALIB;
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else 
			prefix_dir_name = tmp + prefix_len + 1;
		break;
	case GDA_LIBEXEC_DIR:
		tmp = LIBGDALIBEXEC;
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else
			prefix_dir_name = tmp + prefix_len + 1;
		break;
	case GDA_ETC_DIR:
		tmp = LIBGDASYSCONF;
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else
			prefix_dir_name = tmp + prefix_len + 1;
		break;
	}

#ifdef GDA_DEBUG_NO
	g_print ("%s ()\n", __FUNCTION__);
#endif


	if (!prefix) {
		/* prefix part for each OS */
#ifdef G_OS_WIN32
		/* Get from location of libgda DLL */
		GetModuleFileNameW (hdllmodule, path, MAX_PATH);
		prefix = g_utf16_to_utf8 (path, -1, NULL, NULL, NULL);
		if ((p = strrchr (prefix, G_DIR_SEPARATOR)) != NULL)
			*p = '\0';
		
		p = strrchr (prefix, G_DIR_SEPARATOR);
		if (p && (g_ascii_strcasecmp (p + 1, "bin") == 0 ||
			  g_ascii_strcasecmp (p + 1, "lib") == 0))
			*p = '\0';	
#elif HAVE_CARBON
#define MAXLEN 500
		ProcessSerialNumber myProcess;
		FSRef bundleLocation;
		unsigned char bundlePath[MAXLEN];
		
		if ((GetCurrentProcess (&myProcess) == noErr) &&
		    (GetProcessBundleLocation (&myProcess, &bundleLocation) == noErr) &&
		    (FSRefMakePath (&bundleLocation, bundlePath, MAXLEN) == noErr)) {
			prefix = g_path_get_dirname ((const char*) bundlePath);
			if (g_str_has_suffix (prefix, "bin"))
				prefix [strlen (prefix) - 3] = 0;
		}
		else
			g_warning ("Could not get PREFIX (using Mac OS X Carbon)");
#else
		if (!prefix)
			prefix = _gda_gbr_find_prefix (LIBGDAPREFIX);
#endif
	}

	if (!prefix || !*prefix)
		return NULL;
       
	/* filename part */
	size = 10;
	parts = g_new0 (gchar *, size);
	va_start (ap, where);
	for (i = 0, tmp = va_arg (ap, gchar *); tmp; tmp = va_arg (ap, gchar *)) {
		if (i == size - 1) {
			size += 10;
			parts = g_renew (gchar *, parts, size);
		}
		parts[i] = g_strdup (tmp);
#ifdef GDA_DEBUG_NO
		g_print ("\t+ %s\n", tmp);
#endif
		i++;
	}
	parts[i] = NULL;
	va_end (ap);

	file_part = g_build_filenamev (parts);
	g_strfreev (parts);

	/* result */
	if (prefix_dir_name)
		tmp = g_build_filename (prefix, prefix_dir_name, file_part, NULL);
	else
		tmp = g_build_filename (prefix, file_part, NULL);

	if (!g_file_test (tmp, G_FILE_TEST_EXISTS) &&
	    g_str_has_suffix (prefix, "libgda")) {
		/* test if we are in the sources */
		g_free (tmp);
		if (prefix_dir_name)
			tmp = g_build_filename (LIBGDAPREFIX, prefix_dir_name, file_part, NULL);
		else
			tmp = g_build_filename (LIBGDAPREFIX, file_part, NULL);
	}

	g_free (prefix);
	g_free (file_part);
#ifdef GDA_DEBUG_NO
	g_print ("\t=> %s\n", tmp);
#endif

	return tmp;
}
