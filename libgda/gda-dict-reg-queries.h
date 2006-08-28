/* gda-dict-reg-queries.h
 *
 * Copyright (C) 2006 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef __GDA_DICT_REG_QUERIES_H_
#define __GDA_DICT_REG_QUERIES_H_

#include "gda-dict-private.h"

GdaDictRegisterStruct *gda_queries_get_register  ();
guint                  gda_queries_get_serial    (GdaDictRegisterStruct *reg);
void                   gda_queries_declare_serial (GdaDictRegisterStruct *reg, guint id);

#endif
