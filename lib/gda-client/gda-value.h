/* GNOME DB libary
 * Copyright (C) 1998,1999 Michael Lausch
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

#ifndef __gda_value_h__
#define __gda_value_h__ 1


#include "gda.h"

GDA_Value* gda_value_new             (void);
void       gda_value_free            (GDA_Value* val);

void       gda_value_set_gint        (GDA_Value* val, gint i);
void       gda_value_set_guint       (GDA_Value* val, guint ui);
void       gda_value_set_glong       (GDA_Value* val, glong l);
void       gda_value_set_gulong      (GDA_Value* val, gulong ul);
void       gda_value_set_gshort      (GDA_Value* val, gshort s);
void       gda_value_set_gushort     (GDA_Value* val, gushort us);
void       gda_value_set_glong_long  (GDA_Value* val, glonglong ll);
void       gda_value_set_gulong_long (GDA_Value* val, gulonglong ull);

#endif
