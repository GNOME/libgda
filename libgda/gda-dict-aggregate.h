/* gda-dict-aggregate.h
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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


#ifndef __GDA_DICT_AGGREGATE_H_
#define __GDA_DICT_AGGREGATE_H_

#include <libgda/gda-object.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_DICT_AGGREGATE          (gda_dict_aggregate_get_type())
#define GDA_DICT_AGGREGATE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_dict_aggregate_get_type(), GdaDictAggregate)
#define GDA_DICT_AGGREGATE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_dict_aggregate_get_type (), GdaDictAggregateClass)
#define GDA_IS_DICT_AGGREGATE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_dict_aggregate_get_type ())


/* error reporting */
extern GQuark gda_dict_aggregate_error_quark (void);
#define GDA_DICT_AGGREGATE_ERROR gda_dict_aggregate_error_quark ()

enum
{
	GDA_DICT_AGGREGATE_XML_LOAD_ERROR
};


/* struct for the object's data */
struct _GdaDictAggregate
{
	GdaObject                         object;
	GdaDictAggregatePrivate       *priv;
};

/* struct for the object's class */
struct _GdaDictAggregateClass
{
	GdaObjectClass                    parent_class;
};

GType             gda_dict_aggregate_get_type           (void);
GObject          *gda_dict_aggregate_new                (GdaDict *dict);
void              gda_dict_aggregate_set_dbms_id        (GdaDictAggregate *agg, const gchar *id);
gchar            *gda_dict_aggregate_get_dbms_id        (GdaDictAggregate *agg);
void              gda_dict_aggregate_set_sqlname        (GdaDictAggregate *agg, const gchar *sqlname);
const gchar      *gda_dict_aggregate_get_sqlname        (GdaDictAggregate *agg);
void              gda_dict_aggregate_set_arg_dict_type  (GdaDictAggregate *agg, GdaDictType *dt);
GdaDictType      *gda_dict_aggregate_get_arg_dict_type  (GdaDictAggregate *agg);
void              gda_dict_aggregate_set_ret_dict_type  (GdaDictAggregate *agg, GdaDictType *dt);
GdaDictType      *gda_dict_aggregate_get_ret_dict_type  (GdaDictAggregate *agg);

G_END_DECLS

#endif
