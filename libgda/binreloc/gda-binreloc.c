/*
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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

#include "gda-binreloc.h"
/* include source file as mentioned in gbr_init_lib()'s doc */
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
  #ifdef ENABLE_BINRELOC
    #include <Carbon/Carbon.h>
  #endif
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

	/*
	g_print ("LIBGDAPREFIX = %s\n", LIBGDAPREFIX);
	g_print ("LIBGDABIN = %s\n", LIBGDABIN);
	g_print ("LIBGDASBIN = %s\n", LIBGDASBIN);
	g_print ("LIBGDADATA = %s\n",LIBGDADATA );
	g_print ("LIBGDALIB = %s\n", LIBGDALIB);
	g_print ("LIBGDALIBEXEC = %s\n",LIBGDALIBEXEC );
	g_print ("LIBGDASYSCONF = %s\n", LIBGDASYSCONF);
	*/

#ifdef G_OS_WIN32
	wchar_t path[MAX_PATH]; /* Flawfinder: ignore */
	gchar* p;
#endif

	switch (where) {
	default:
	case GDA_NO_DIR:
		break;
	case GDA_BIN_DIR:
		tmp = LIBGDABIN;
#ifndef G_OS_WIN32
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else
			prefix_dir_name = tmp + prefix_len + 1;
#else
		prefix_dir_name = "bin";
#endif
		break;
	case GDA_SBIN_DIR:
		tmp = LIBGDASBIN;
#ifndef G_OS_WIN32
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else
			prefix_dir_name = tmp + prefix_len + 1;
#else
		prefix_dir_name = "sbin";
#endif
		break;
	case GDA_DATA_DIR:
		tmp = LIBGDADATA;
#ifndef G_OS_WIN32
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else
			prefix_dir_name = tmp + prefix_len + 1;
#else
		prefix_dir_name = "share";
#endif		
		break;
	case GDA_LOCALE_DIR:
		tmp = LIBGDADATA;
#ifndef G_OS_WIN32
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) {
			prefix = g_strdup (tmp);
			prefix_dir_name = "locale";
		}
		else
			prefix_dir_name = "share" G_DIR_SEPARATOR_S "locale";
#else
		prefix_dir_name = "share" G_DIR_SEPARATOR_S "locale";
#endif
		break;
	case GDA_LIB_DIR:
		tmp = LIBGDALIB;
#ifndef G_OS_WIN32
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else 
			prefix_dir_name = tmp + prefix_len + 1;
#else
		prefix_dir_name = "lib";
#endif
		break;
	case GDA_LIBEXEC_DIR:
		tmp = LIBGDALIBEXEC;
#ifndef G_OS_WIN32
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else
			prefix_dir_name = tmp + prefix_len + 1;
#else
		prefix_dir_name = "libexec";
#endif
		break;
	case GDA_ETC_DIR:
		tmp = LIBGDASYSCONF;
#ifndef G_OS_WIN32
		if (! g_str_has_prefix (tmp, LIBGDAPREFIX) || (tmp [prefix_len] != G_DIR_SEPARATOR)) 
			prefix = g_strdup (tmp);
		else
			prefix_dir_name = tmp + prefix_len + 1;
#else
		prefix_dir_name = "etc";
#endif
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
  #ifdef ENABLE_BINRELOC
    #define MAXLEN 500
		ProcessSerialNumber myProcess;
		FSRef bundleLocation;
		unsigned char bundlePath[MAXLEN]; /* Flawfinder: ignore */
		
		if ((GetCurrentProcess (&myProcess) == noErr) &&
		    (GetProcessBundleLocation (&myProcess, &bundleLocation) == noErr) &&
		    (FSRefMakePath (&bundleLocation, bundlePath, MAXLEN) == noErr)) {
			if (g_str_has_suffix ((const gchar*) bundlePath, ".app"))
				prefix = g_strdup_printf ("%s/Contents/Resources", (const gchar*) bundlePath);
			else {
				prefix = g_path_get_dirname ((const char*) bundlePath);
				if (g_str_has_suffix (prefix, "bin"))
					prefix [strlen (prefix) - 3] = 0;
			}
#ifdef GDA_DEBUG_NO
			g_print ("BUNDLE=%s, prefix=%s\n", bundlePath, prefix);
#endif
		}
		else
			g_warning ("Could not get PREFIX (using Mac OS X Carbon)");
  #else
#ifdef GDA_DEBUG_NO
		g_print ("Binreloc disabled, using %s\n", LIBGDAPREFIX);
#endif
  #endif

		if (!prefix)
			prefix = g_strdup (LIBGDAPREFIX);
#else
		if (!prefix)
		 	prefix = _gda_gbr_find_prefix (LIBGDAPREFIX);
#endif
	}

	if (!prefix || !*prefix) {
		g_free (prefix);
		return NULL;
	}
       
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

	if (!g_file_test (tmp, G_FILE_TEST_EXISTS)) {
		if (g_str_has_suffix (prefix, "libgda")) {
			/* test if we are in the sources */
			g_free (tmp);
			if (prefix_dir_name)
				tmp = g_build_filename (LIBGDAPREFIX, prefix_dir_name, file_part, NULL);
			else
				tmp = g_build_filename (LIBGDAPREFIX, file_part, NULL);
		}
		else {
			g_free (prefix);
			prefix = g_strdup (LIBGDAPREFIX);
			g_free (tmp);
			if (prefix_dir_name)
				tmp = g_build_filename (prefix, prefix_dir_name, file_part, NULL);
			else
				tmp = g_build_filename (prefix, file_part, NULL);
		}
	}

	g_free (prefix);
	g_free (file_part);
#ifdef GDA_DEBUG_NO
	g_print ("\t=> %s\n", tmp);
#endif

	return tmp;
}
