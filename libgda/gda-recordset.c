/* GDA libary
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <config.h>
#include <libgda/gda-recordset.h>
#include <gobject/gsignal.h>

enum {
	LAST_SIGNAL
};

static gint gda_recordset_signals[LAST_SIGNAL] = { 0, };

static void gda_recordset_class_init (GdaRecordsetClass *);
static void gda_recordset_init       (GdaRecordset *, GdaRecordsetClass *);
static void gda_recordset_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaRecordset class implementation
 */

GType
gda_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaRecordset),
			0,
			(GInstanceInitFunc) gda_recordset_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaRecordset", &info, 0);
	}
	return type;
}

static void
gda_recordset_class_init (GdaRecordsetClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_recordset_finalize;
}

static void
gda_recordset_init (GdaRecordset *rs, GdaRecordsetClass *klass)
{
}

static void
gda_recordset_finalize (GObject * object)
{
	GdaRecordset *rs = (GdaRecordset *) object;

	parent_class->finalize (object);
}

/**
 * gda_recordset_new:
 *
 * Allocates space for a new recordset.
 *
 * Returns: the allocated recordset object
 */
GdaRecordset *
gda_recordset_new (void)
{
	return GDA_RECORDSET (g_object_new (GDA_TYPE_RECORDSET, NULL));
}

