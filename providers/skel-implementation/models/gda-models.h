/*
 * Copyright (C) YEAR The GNOME Foundation
 *
 * AUTHORS:
 *      TO_ADD: your name and email
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_MODELS_H__
#define __GDA_MODELS_H__

/*
 * Provider name
 */
#define MODELS_PROVIDER_NAME "Models"

/* TO_ADD: include headers necessary for the C or C++ API */

/*
 * Provider's specific connection data
 */
typedef struct {
	/* TO_ADD: this structure holds any information necessary to specialize the GdaConnection, usually a connection
	 * handle from the C or C++ API
	 */
	int foo;
} ModelsConnectionData;

#endif
