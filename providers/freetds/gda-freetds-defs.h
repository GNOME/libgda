/* GDA FreeTDS Provider
 * Copyright (C) 2005 The GNOME Foundation
 *
 * AUTHORS: Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(__gda_freetds_defs_h__)
#  define __gda_freetds_defs_h__

#define FREETDS_VERSION (10000*FREETDS_VERSION_MAJOR+100*FREETDS_VERSION_MINOR)

#if FREETDS_VERSION >= 6100 && FREETDS_VERSION < 6300
#define _TDSCONNECTINFO TDSCONNECTINFO
#define _TDSCOLINFO     TDSCOLINFO
#define _TDSMSGINFO     TDSMSGINFO
#else
  #if FREETDS_VERSION >= 6300
#define _TDSCONNECTINFO TDSCONNECTION
#define _TDSCOLINFO     TDSCOLUMN
#define _TDSMSGINFO     TDSMESSAGE
  #else
#define _TDSCONNECTINFO TDSCONFIGINFO
#define _TDSCOLINFO     TDSCOLUMN
#define _TDSMSGINFO     TDSMSGINFO
  #endif
#endif


#endif
