/* XmlQuery: the xml query object
 * Copyright (C) 2000 Gerhard Dieringer
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <gtk/gtk.h>


#include "xml-query.h"
#include "utils.h"
#include "read.h"

extern XmlQueryItem *construct_insert (void);
extern XmlQueryItem *construct_delete (void);
extern XmlQueryItem *construct_update (void);
extern XmlQueryItem *construct_select (void);




gint
main (gint ac, gchar * av[])
{
	xmlNode *node;
	XmlQueryItem *item;
	gchar *item_string;

	if (ac < 3)
		return 0;

	gtk_type_init ();

	switch (*(av[2])) {
	case 'r':
		if (ac < 4)
			return 0;
		item = proc_file (av[3]);
		break;
	case 'i':
		item = construct_insert ();
		break;
	case 'd':
		item = construct_delete ();
		break;
	case 'u':
		item = construct_update ();
		break;
	case 's':
		item = construct_select ();
		break;
	default:
		return 0;
	}

	node = xml_query_item_to_dom (item, NULL);

	switch (*(av[1])) {
	case 'x':
		item_string = xml_query_dom_to_xml (node, TRUE);
		break;
	case 's':
		item_string = xml_query_dom_to_sql (node, TRUE);
		break;
	default:
		return 0;
	}

	g_print (item_string);
	g_print ("\n");
	return 0;
}



/*
insert into tab_a (a,b,c) VALUES (1,f(1,"x"),"abc")
*/

XmlQueryItem *
construct_insert (void)
{
	XmlQueryItem *insert, *func;
	gchar *table;

	insert = xml_query_insert_new ();

	func = xml_query_func_new_with_data ("f", NULL, FALSE);

	xml_query_func_add_const_from_text (XML_QUERY_FUNC (func),
					    "1", "int", FALSE);

	xml_query_func_add_field_from_text (XML_QUERY_FUNC (func),
					    NULL, "x", NULL);

	table = xml_query_dml_add_target_from_text (XML_QUERY_DML (insert),
						    "tab_a", NULL);


	xml_query_dml_add_field_from_text (XML_QUERY_DML (insert),
					   table, "a", NULL, FALSE);

	xml_query_dml_add_const_from_text (XML_QUERY_DML (insert),
					   "1", "int", FALSE);


	xml_query_dml_add_field_from_text (XML_QUERY_DML (insert),
					   table, "b", NULL, FALSE);

	xml_query_dml_add_func (XML_QUERY_DML (insert), func);



	xml_query_dml_add_field_from_text (XML_QUERY_DML (insert),
					   table, "c", NULL, FALSE);

	xml_query_dml_add_const_from_text (XML_QUERY_DML (insert),
					   "abc", "char", FALSE);

	return xml_query_query_new_with_data (insert);
}




/*
delete from tab_a where (x = 'a') and ((y = 'b') or (z = 'c'))
*/

XmlQueryItem *
construct_delete (void)
{
	XmlQueryItem *delete, *left, *right, *top, *and, *or;
	gchar *table;

	delete = xml_query_delete_new ();

	table = xml_query_dml_add_target_from_text (XML_QUERY_DML (delete),
						    "tab_a", NULL);

	left = xml_query_field_new_with_data (NULL, "x", NULL);
	right = xml_query_const_new_with_data ("a", NULL, "char", FALSE);
	top = xml_query_dual_new_eq_with_data (left, right);

	and = xml_query_list_new_and ();
	xml_query_item_add (and, top);

	left = xml_query_field_new_with_data (NULL, "y", NULL);
	right = xml_query_const_new_with_data ("b", NULL, "char", FALSE);
	top = xml_query_dual_new_eq_with_data (left, right);

	or = xml_query_list_new_or ();
	xml_query_item_add (or, top);

	left = xml_query_field_new_with_data (NULL, "z", NULL);
	right = xml_query_const_new_with_data ("c", NULL, "char", FALSE);
	top = xml_query_dual_new_eq_with_data (left, right);

	xml_query_item_add (or, top);
	xml_query_item_add (and, or);

	xml_query_dml_add_row_condition (XML_QUERY_DML (delete), and, "and");

	return xml_query_query_new_with_data (delete);
}




/*
update tab_a set x = 2, a = 'abc' where x is null
*/

XmlQueryItem *
construct_update (void)
{
	XmlQueryItem *update, *field, *qual;
	gchar *table;

	update = xml_query_update_new ();

	table = xml_query_dml_add_target_from_text (XML_QUERY_DML (update),
						    "tab_a", NULL);

	xml_query_dml_add_set_const (XML_QUERY_DML (update),
				     "x", "2", "int", FALSE);

	xml_query_dml_add_set_const (XML_QUERY_DML (update),
				     "a", "abc", "char", FALSE);


	field = xml_query_field_new_with_data (NULL, "x", NULL);
	qual = xml_query_bin_new_null_with_data (field);

	xml_query_dml_add_row_condition (XML_QUERY_DML (update), qual, "and");

	return xml_query_query_new_with_data (update);
}





/*
 select a, b, sum(c), "abc"
   from tab_a
  where not x is null
    and a <> b
 having sum(c) > 0
  group by a, b, "abc"
  order by 2, 1 desc
*/

XmlQueryItem *
construct_select (void)
{
	XmlQueryItem *select, *func, *field1, *field2, *qual, *cnst;
	XmlQueryStack *stack;
	gchar *table;

	stack = xml_query_stack_new ();

	select = xml_query_select_new ();

	table = xml_query_dml_add_target_from_text (XML_QUERY_DML (select),
						    "tab_a", NULL);
	xml_query_dml_add_field_from_text (XML_QUERY_DML (select),
					   table, "a", NULL, TRUE);
	xml_query_dml_add_field_from_text (XML_QUERY_DML (select),
					   table, "b", NULL, TRUE);
	func = xml_query_func_new_with_data ("sum", NULL, "yes");
	xml_query_func_add_field_from_text (XML_QUERY_FUNC (func),
					    table, "c", NULL);
	xml_query_dml_add_func (XML_QUERY_DML (select), func);

	xml_query_dml_add_const_from_text (XML_QUERY_DML (select),
					   "abc", "char", FALSE);
	field1 = xml_query_field_new_with_data (NULL, "x", NULL);
	qual = xml_query_bin_new_null_with_data (field1);
	qual = xml_query_bin_new_not_with_data (qual);
	xml_query_dml_add_row_condition (XML_QUERY_DML (select), qual, "and");

	field1 = xml_query_field_new_with_data (NULL, "a", NULL);
	field2 = xml_query_field_new_with_data (NULL, "b", NULL);

	xml_query_stack_push (stack, field1);
	xml_query_stack_push (stack, field2);
	field2 = xml_query_stack_pop (stack);
	field1 = xml_query_stack_pop (stack);

	qual = xml_query_dual_new_ne_with_data (field1, field2);

	xml_query_dml_add_row_condition (XML_QUERY_DML (select), qual, "and");

	cnst = xml_query_const_new_with_data ("0", NULL, "int", "no");
	qual = xml_query_dual_new_gt_with_data (func, cnst);
	xml_query_dml_add_group_condition (XML_QUERY_DML (select),
					   qual, "and");

	xml_query_dml_add_order (XML_QUERY_DML (select), 2, TRUE);
	xml_query_dml_add_order (XML_QUERY_DML (select), 1, FALSE);

	return xml_query_query_new_with_data (select);
}
