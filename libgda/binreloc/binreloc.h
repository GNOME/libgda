/*
 * BinReloc - a library for creating relocatable executables
 * Written by: Hongli Lai <h.lai@chello.nl>
 * http://autopackage.org/
 *
 * This source code is public domain. You can relicense this code
 * under whatever license you want.
 *
 * See http://autopackage.org/docs/binreloc/ for
 * more information and how to use this.
 */

/*
 * To avoid name clashes with other libraries which might also use BinReloc, all the API
 * has been prefixed with _gda
 */

#ifndef __BINRELOC_H__
#define __BINRELOC_H__

#include <glib.h>

G_BEGIN_DECLS


/** These error codes can be returned by br_init(), br_init_lib(), gbr_init() or gbr_init_lib(). */
typedef enum {
	/** Cannot allocate memory. */
	_GDA_GBR_INIT_ERROR_NOMEM,
	/** Unable to open /proc/self/maps; see errno for details. */
	_GDA_GBR_INIT_ERROR_OPEN_MAPS,
	/** Unable to read from /proc/self/maps; see errno for details. */
	_GDA_GBR_INIT_ERROR_READ_MAPS,
	/** The file format of /proc/self/maps is invalid; kernel bug? */
	_GDA_GBR_INIT_ERROR_INVALID_MAPS,
	/** BinReloc is disabled (the ENABLE_BINRELOC macro is not defined). */
	_GDA_GBR_INIT_ERROR_DISABLED
} GbrInitError;


#ifndef BINRELOC_RUNNING_DOXYGEN
/* Mangle symbol names to avoid symbol collisions with other ELF objects. */
	#define _gda_gbr_find_exe         fnYM49765777344607__gda_gbr_find_exe
	#define _gda_gbr_find_exe_dir     fnYM49765777344607__gda_gbr_find_exe_dir
	#define _gda_gbr_find_prefix      fnYM49765777344607__gda_gbr_find_prefix
	#define _gda_gbr_find_bin_dir     fnYM49765777344607__gda_gbr_find_bin_dir
	#define _gda_gbr_find_sbin_dir    fnYM49765777344607__gda_gbr_find_sbin_dir
	#define _gda_gbr_find_data_dir    fnYM49765777344607__gda_gbr_find_data_dir
	#define _gda_gbr_find_locale_dir  fnYM49765777344607__gda_gbr_find_locale_dir
	#define _gda_gbr_find_lib_dir     fnYM49765777344607__gda_gbr_find_lib_dir
	#define _gda_gbr_find_libexec_dir fnYM49765777344607__gda_gbr_find_libexec_dir
	#define _gda_gbr_find_etc_dir     fnYM49765777344607__gda_gbr_find_etc_dir


#endif
gboolean _gda_gbr_init             (GError **error);
gboolean _gda_gbr_init_lib         (GError **error);

gchar   *_gda_gbr_find_exe         (const gchar *default_exe);
gchar   *_gda_gbr_find_exe_dir     (const gchar *default_dir);
gchar   *_gda_gbr_find_prefix      (const gchar *default_prefix);
gchar   *_gda_gbr_find_bin_dir     (const gchar *default_bin_dir);
gchar   *_gda_gbr_find_sbin_dir    (const gchar *default_sbin_dir);
gchar   *_gda_gbr_find_data_dir    (const gchar *default_data_dir);
gchar   *_gda_gbr_find_locale_dir  (const gchar *default_locale_dir);
gchar   *_gda_gbr_find_lib_dir     (const gchar *default_lib_dir);
gchar   *_gda_gbr_find_libexec_dir (const gchar *default_libexec_dir);
gchar   *_gda_gbr_find_etc_dir     (const gchar *default_etc_dir);


G_END_DECLS

#endif /* __BINRELOC_H__ */
