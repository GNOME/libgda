/* gda-ddl-fkey.h
 *
 * Copyright (C) 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#ifndef GDA_DDL_FKEY_H
#define GDA_DDL_FKEY_H

#include <glib-object.h>
#include <gmodule.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>
#include "gda-ddl-buildable.h"
#include "gda-server-operation.h"
#include "gda-meta-struct.h"

G_BEGIN_DECLS

#define GDA_TYPE_DDL_FKEY (gda_ddl_fkey_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDdlFkey, gda_ddl_fkey, GDA, DDL_FKEY, GObject)

struct _GdaDdlFkeyClass {
    GObjectClass parent_class;
};

/**
 * GdaDdlFkeyReferenceAction:
 * @GDA_DDL_FKEY_NO_ACTION: Action is not specified. 
 * @GDA_DDL_FKEY_SET_NULL: Action value is set to %NULL
 * @GDA_DDL_FKEY_RESTRICT: Value is set to "RESTRICT"
 * @GDA_DDL_FKEY_SET_DEFAULT: Value is set to default behavior  
 * @GDA_DDL_FKEY_CASCADE: Value is set to cascade
 * 
 * Specify numeric value for the actions, e.g. "ON DELETE" and "ON UPDATE"
 */
typedef enum {
    GDA_DDL_FKEY_NO_ACTION,
    GDA_DDL_FKEY_SET_NULL,
    GDA_DDL_FKEY_RESTRICT,
    GDA_DDL_FKEY_SET_DEFAULT,
    GDA_DDL_FKEY_CASCADE,
    GDA_DDL_FKEY_ERROR
} GdaDdlFkeyReferenceAction;


GdaDdlFkey*       gda_ddl_fkey_new             (void);

const GList*      gda_ddl_fkey_get_field_name  (GdaDdlFkey *self);
const GList*      gda_ddl_fkey_get_ref_field   (GdaDdlFkey *self);

void              gda_ddl_fkey_set_field       (GdaDdlFkey  *self,
                                                const gchar *field,
                                                const gchar *reffield);

const gchar*      gda_ddl_fkey_get_ref_table   (GdaDdlFkey *self);
void              gda_ddl_fkey_set_ref_table   (GdaDdlFkey  *self,
                                                const gchar *rtable);

const gchar*      gda_ddl_fkey_get_ondelete    (GdaDdlFkey *self);

GdaDdlFkeyReferenceAction
                  gda_ddl_fkey_get_ondelete_id (GdaDdlFkey *self);

void              gda_ddl_fkey_set_ondelete    (GdaDdlFkey *self,
                                                GdaDdlFkeyReferenceAction id);

const gchar*      gda_ddl_fkey_get_onupdate    (GdaDdlFkey *self);

GdaDdlFkeyReferenceAction
                  gda_ddl_fkey_get_onupdate_id (GdaDdlFkey *self);

void              gda_ddl_fkey_set_onupdate    (GdaDdlFkey *self,
                                                GdaDdlFkeyReferenceAction id);

gboolean          gda_ddl_fkey_prepare_create  (GdaDdlFkey *self,
                                                GdaServerOperation *op,
                                                GError **error);
GdaDdlFkey       *gda_ddl_fkey_new_from_meta   (GdaMetaTableForeignKey *metafkey);

G_END_DECLS

#endif /* GDA_DDL_FKEY_H */

