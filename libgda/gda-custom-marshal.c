/* GDA common library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Johannes Schmid
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#define G_LOG_DOMAIN "GDA-custom-marshal"

#include <gda-custom-marshal.h>


/* These marshallers are hardcoded here because glib-genmarshal does not support the marshalled types */

void
_gda_marshal_VOID__GTYPE_GTYPE (GClosure     *closure,
                               GValue       *return_value G_GNUC_UNUSED,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint G_GNUC_UNUSED,
                               gpointer      marshal_data)
{
	typedef void (*GMarshalFunc_VOID__GTYPE_GTYPE) (gpointer     data1,
							GType        arg_1,
							GType        arg_2,
							gpointer     data2);
	register GMarshalFunc_VOID__GTYPE_GTYPE callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	
	g_return_if_fail (n_param_values == 3);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_VOID__GTYPE_GTYPE) (marshal_data ? marshal_data : cc->callback);
	
	callback (data1,
		  g_value_get_gtype (param_values + 1),
		  g_value_get_gtype (param_values + 2),
		  data2);
}

void
_gda_marshal_ERROR__OBJECT_VALUE (GClosure     *closure,
				 GValue       *return_value G_GNUC_UNUSED,
				 guint         n_param_values,
				 const GValue *param_values,
				 gpointer      invocation_hint G_GNUC_UNUSED,
				 gpointer      marshal_data)
{
	typedef GError *(*GMarshalFunc_ERROR__OBJECT_VALUE) (gpointer     data1,
							     gpointer     arg_1,
							     gpointer     arg_2,
							     gpointer     data2);
	register GMarshalFunc_ERROR__OBJECT_VALUE callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	GError *v_return;
	
	g_return_if_fail (n_param_values == 3);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_ERROR__OBJECT_VALUE) (marshal_data ? marshal_data : cc->callback);
	
	v_return = callback (data1,
			     g_value_get_object (param_values + 1),
			     g_value_get_boxed (param_values + 2),
			     data2);
	g_value_take_boxed (return_value, v_return);
}

void
_gda_marshal_VOID__OBJECT_STRING_VALUE (GClosure     *closure,
				       GValue       *return_value G_GNUC_UNUSED,
				       guint         n_param_values,
				       const GValue *param_values,
				       gpointer      invocation_hint G_GNUC_UNUSED,
				       gpointer      marshal_data)
{
	typedef void (*GMarshalFunc_VOID__OBJECT_STRING_VALUE) (gpointer     data1,
								gpointer     arg_1,
								gpointer     arg_2,
								gpointer     arg_3,
								gpointer     data2);
	register GMarshalFunc_VOID__OBJECT_STRING_VALUE callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	
	g_return_if_fail (n_param_values == 4);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_VOID__OBJECT_STRING_VALUE) (marshal_data ? marshal_data : cc->callback);
	
	callback (data1,
		  g_value_get_object (param_values + 1),
		  (gpointer) g_value_get_string (param_values + 2),
		  g_value_get_boxed (param_values + 3),
		  data2);
}

void
_gda_marshal_ERROR__VOID (GClosure     *closure,
			 GValue       *return_value G_GNUC_UNUSED,
			 guint         n_param_values,
			 const GValue *param_values,
			 gpointer      invocation_hint G_GNUC_UNUSED,
			 gpointer      marshal_data)
{
	typedef GError *(*GMarshalFunc_ERROR__VOID) (gpointer     data1,
						     gpointer     data2);
	register GMarshalFunc_ERROR__VOID callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	GError *v_return;
	
	g_return_if_fail (n_param_values == 1);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_ERROR__VOID) (marshal_data ? marshal_data : cc->callback);
	
	v_return = callback (data1, data2);
	g_value_take_boxed (return_value, v_return);	
}

void
_gda_marshal_VOID__STRING_VALUE (GClosure     *closure,
				GValue       *return_value G_GNUC_UNUSED,
				guint         n_param_values,
				const GValue *param_values,
				gpointer      invocation_hint G_GNUC_UNUSED,
				gpointer      marshal_data)
{
	typedef void (*GMarshalFunc_VOID__STRING_VALUE) (gpointer     data1,
							 gpointer     arg_1,
							 gpointer     arg_2,
							 gpointer     data2);
	register GMarshalFunc_VOID__STRING_VALUE callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	
	g_return_if_fail (n_param_values == 3);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_VOID__STRING_VALUE) (marshal_data ? marshal_data : cc->callback);
	
	callback (data1,
		  (gpointer) g_value_get_string (param_values + 1),
		  g_value_get_boxed (param_values + 2),
		  data2);
}

void
_gda_marshal_ERROR__VALUE (GClosure     *closure,
			  GValue       *return_value G_GNUC_UNUSED,
			  guint         n_param_values,
			  const GValue *param_values,
			  gpointer      invocation_hint G_GNUC_UNUSED,
			  gpointer      marshal_data)
{
	typedef GError *(*GMarshalFunc_ERROR__VALUE) (gpointer     data1,
						      gpointer     arg_1,
						      gpointer     data2);
	register GMarshalFunc_ERROR__VALUE callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	GError *v_return;
	
	g_return_if_fail (n_param_values == 2);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_ERROR__VALUE) (marshal_data ? marshal_data : cc->callback);
	
	v_return = callback (data1,
			     g_value_get_boxed (param_values + 1),
			     data2);
	g_value_take_boxed (return_value, v_return);
}

void
_gda_marshal_ERROR__INT_INT (GClosure     *closure,
			    GValue       *return_value G_GNUC_UNUSED,
			    guint         n_param_values,
			    const GValue *param_values,
			    gpointer      invocation_hint G_GNUC_UNUSED,
			    gpointer      marshal_data)
{
	typedef GError *(*GMarshalFunc_ERROR__INT_INT) (gpointer     data1,
							gint         arg_1,
							gint         arg_2,
							gpointer     data2);
	register GMarshalFunc_ERROR__INT_INT callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	GError *v_return;
	
	g_return_if_fail (n_param_values == 3);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_ERROR__INT_INT) (marshal_data ? marshal_data : cc->callback);
	
	v_return = callback (data1,
			     g_value_get_int (param_values + 1),
			     g_value_get_int (param_values + 2),
			     data2);
	g_value_take_boxed (return_value, v_return);
}

void
_gda_marshal_VOID__SLIST (GClosure     *closure,
			  GValue       *return_value G_GNUC_UNUSED,
			  guint         n_param_values,
			  const GValue *param_values,
			  gpointer      invocation_hint G_GNUC_UNUSED,
			  gpointer      marshal_data)
{
	typedef void (*GMarshalFunc_VOID__SLIST) (gpointer     data1,
						  gpointer     arg_1,
						  gpointer     data2);
	register GMarshalFunc_VOID__SLIST callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	
	g_return_if_fail (n_param_values == 2);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_VOID__SLIST) (marshal_data ? marshal_data : cc->callback);
	
	callback (data1,
		  g_value_get_pointer (param_values + 1),
		  data2);
}

void
_gda_marshal_ERROR__METACONTEXT (GClosure     *closure,
				 GValue       *return_value G_GNUC_UNUSED,
				 guint         n_param_values,
				 const GValue *param_values,
				 gpointer      invocation_hint G_GNUC_UNUSED,
				 gpointer      marshal_data)
{
	typedef GError *(*GMarshalFunc_ERROR__METACONTEXT) (gpointer     data1,
							    gpointer     arg_1,
							    gpointer     data2);
	register GMarshalFunc_ERROR__METACONTEXT callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	GError *v_return;
	
	g_return_if_fail (n_param_values == 2);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_ERROR__METACONTEXT) (marshal_data ? marshal_data : cc->callback);
	
	v_return = callback (data1,
			     g_value_get_pointer (param_values + 1),
			     data2);
	g_value_take_boxed (return_value, v_return);	
}
