/* 
 * Copyright (C) 2007 - 2009 Vivien Malerba
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

#ifndef _GDA_DEBUG_MACROS_H_
#define _GDA_DEBUG_MACROS_H_

#include <glib.h>
#include <glib-object.h>

#ifdef GDA_DEBUG
#define D_COL_NOR "\033[0m"
#define D_COL_H0 "\033[;34;7m"
#define D_COL_H1 "\033[;36;7m"
#define D_COL_H2 "\033[;36;4m"
#define D_COL_OK "\033[;32m"
#define D_COL_ERR "\033[;31;1m"
#endif

#ifndef TO_IMPLEMENT
  #ifdef GDA_DEBUG
    #define TO_IMPLEMENT g_print (D_COL_ERR "Implementation missing:" D_COL_NOR " %s() in %s line %d\n", __FUNCTION__, __FILE__,__LINE__)
  #else
    #define TO_IMPLEMENT g_print ("Implementation missing: %s() in %s line %d\n", __FUNCTION__, __FILE__,__LINE__)
  #endif
#endif

#endif
