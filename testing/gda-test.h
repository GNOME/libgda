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

#if !defined(__gda_test_h__)
#  define __gda_test_h__

#include <libgda/libgda.h>
#include <bonobo/bonobo-i18n.h>

G_BEGIN_DECLS

#define DISPLAY_MESSAGE(_msg_) \
        g_print ("=========================================\n"); \
	g_print ("= %s\n", _msg_); \
        g_print ("=========================================\n");

void test_config (void);

G_END_DECLS

#endif
