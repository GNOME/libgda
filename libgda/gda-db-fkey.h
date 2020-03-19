/* gda-db-fkey.h
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
#ifndef GDA_DB_FKEY_H
#define GDA_DB_FKEY_H

#include <glib-object.h>
#include <gmodule.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>
#include "gda-db-buildable.h"
#include "gda-server-operation.h"

G_BEGIN_DECLS

#define GDA_TYPE_DB_FKEY (gda_db_fkey_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDbFkey, gda_db_fkey, GDA, DB_FKEY, GObject)

struct _GdaDbFkeyClass {
    GObjectClass parent_class;
};

/**
 * GdaDbFkeyReferenceAction:
 * @GDA_DB_FKEY_NO_ACTION: Action is not specified. 
 * @GDA_DB_FKEY_SET_NULL: Action value is set to %NULL
 * @GDA_DB_FKEY_RESTRICT: Value is set to "RESTRICT"
 * @GDA_DB_FKEY_SET_DEFAULT: Value is set to default behavior  
 * @GDA_DB_FKEY_CASCADE: Value is set to cascade
 * 
 * Specify numeric value for the actions, e.g. "ON DELETE" and "ON UPDATE"
 */
typedef enum {
    GDA_DB_FKEY_NO_ACTION,
    GDA_DB_FKEY_SET_NULL,
    GDA_DB_FKEY_RESTRICT,
    GDA_DB_FKEY_SET_DEFAULT,
    GDA_DB_FKEY_CASCADE
} GdaDbFkeyReferenceAction;


GdaDbFkey*        gda_db_fkey_new             (void);

const GList*      gda_db_fkey_get_field_name  (GdaDbFkey *self);
const GList*      gda_db_fkey_get_ref_field   (GdaDbFkey *self);

void              gda_db_fkey_set_field       (GdaDbFkey  *self,
                                               const gchar *field,
                                               const gchar *reffield);

const gchar*      gda_db_fkey_get_ref_table   (GdaDbFkey *self);
void              gda_db_fkey_set_ref_table   (GdaDbFkey  *self,
                                               const gchar *rtable);

const gchar*      gda_db_fkey_get_ondelete    (GdaDbFkey *self);

GdaDbFkeyReferenceAction
                  gda_db_fkey_get_ondelete_id (GdaDbFkey *self);

void              gda_db_fkey_set_ondelete    (GdaDbFkey *self,
                                               GdaDbFkeyReferenceAction id);

const gchar*      gda_db_fkey_get_onupdate    (GdaDbFkey *self);

GdaDbFkeyReferenceAction
                  gda_db_fkey_get_onupdate_id (GdaDbFkey *self);

void              gda_db_fkey_set_onupdate    (GdaDbFkey *self,
                                               GdaDbFkeyReferenceAction id);

gboolean          gda_db_fkey_prepare_create  (GdaDbFkey *self,
                                               GdaServerOperation *op,
                                               gint i,
                                               GError **error);

G_END_DECLS

#endif /* GDA_DB_FKEY_H */

