/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 2000-2001 Rodrigo Moya
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

#if !defined(__gda_command_h__)
#  define __gda_command_h__

#include <orbit/orbit.h>
#include <GNOME_Database.h>
#include <gda-common-defs.h>

G_BEGIN_DECLS

/* The command object. Holds and executes an SQL query or other,
 * datasource specific operations.
 */

typedef struct _GdaCommand      GdaCommand;
typedef struct _GdaCommandClass GdaCommandClass;

#include <gda-recordset.h>	/* these two need the definitions above */
#include <gda-connection.h>

#define GDA_TYPE_COMMAND            (gda_command_get_type())
#define GDA_COMMAND(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_COMMAND, GdaCommand))
#define GDA_COMMAND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_COMMAND, GdaCommandClass))
#define GDA_IS_COMMAND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_COMMAND))
#define GDA_IS_COMMAND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_COMMAND))

struct _GdaCommand {
	GObject object;

	CORBA_Object command;
	CORBA_ORB orb;
	GdaConnection *connection;
	gchar *text;
	GNOME_Database_CommandType type;
	GList *parameters;
	gboolean text_pending;
	gboolean type_pending;
};

struct _GdaCommandClass {
	GObjectClass parent_class;
};

GType           gda_command_get_type (void);

GdaCommand     *gda_command_new (void);
void            gda_command_free (GdaCommand * cmd);
GdaConnection  *gda_command_get_connection (GdaCommand * cmd);
void            gda_command_set_connection (GdaCommand * cmd,
					    GdaConnection * cnc);
gchar          *gda_command_get_text (GdaCommand * cmd);
void            gda_command_set_text (GdaCommand * cmd, gchar * text);
GNOME_Database_CommandType gda_command_get_cmd_type (GdaCommand * cmd);
void            gda_command_set_cmd_type (GdaCommand * cmd,
					  GNOME_Database_CommandType type);
GdaRecordset   *gda_command_execute (GdaCommand * cmd,
				     gulong * reccount, gulong flags);
void            gda_command_create_parameter (GdaCommand * cmd, gchar * name,
					      GNOME_Database_ParameterDirection inout,
					      GNOME_Database_Value * value);

G_END_DECLS

#endif
