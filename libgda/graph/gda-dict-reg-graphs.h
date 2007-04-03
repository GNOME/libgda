/* gda-dict-reg-graphs.h
 *
 * Copyright (C) 2006 Vivien Malerba
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


#ifndef __GDA_DICT_REG_GRAPHS_H_
#define __GDA_DICT_REG_GRAPHS_H_

#include <libgda/gda-dict-private.h>

GdaDictRegisterStruct *gda_graphs_get_register   ();
guint                  gda_graphs_get_serial     (GdaDictRegisterStruct *reg);
void                   gda_graphs_declare_serial (GdaDictRegisterStruct *reg, guint id);

GSList                *gda_graphs_get_with_type  (GdaDict *dict, GdaGraphType type_of_graphs);
GdaGraph              *gda_graphs_get_graph_for_object (GdaDict *dict, GObject *obj);

#endif
