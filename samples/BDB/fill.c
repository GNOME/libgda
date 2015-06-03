/*
 * Copyright (C) 2007 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"

#include "common.h"

DB *dbp;

static int
store (char *color, int type, float size, char *name)
{
	int ret;
	Key m_key;
	Value m_value;
	DBT key, data;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	strncpy (m_key.color, color, COLORSIZE);
	m_key.type = type;
	m_value.size = size;
	strncpy (m_value.name, name, NAMESIZE);

	key.size = sizeof(Key);
	data.size = sizeof(Value);
	key.data = &m_key;
	data.data = &m_value;

	/* Store a key/data pair. */
	if ((ret = dbp->put(dbp, NULL, &key, &data, 0)) == 0) {
		printf("db: %s: key stored.\n", (char *)key.data);
		return 1;
	}
	else {
		dbp->err(dbp, ret, "DB->put");
		return 0;
	}
}

int
main()
{
	int ret, t_ret;


	/* Create the database handle and open the underlying database. */
	if ((ret = db_create(&dbp, NULL, 0)) != 0) {
		fprintf(stderr, "db_create: %s\n", db_strerror(ret));
		exit (1);
	}
	if ((ret = dbp->open(dbp,
			     NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
		dbp->err(dbp, ret, "%s", DATABASE);
		goto err;
	}

	
	if (!store ("green", 1, 1.2, "pear"))
		goto err;
	if (!store ("red", 2, 2.2, "apple"))
		goto err;
	if (!store ("yellow", 3, 3.2, "lemon"))
		goto err;
	if (!store ("blue", 4, 4.2, "orange"))
		goto err;

 err:	if ((t_ret = dbp->close(dbp, 0)) != 0 && ret == 0)
		ret = t_ret;

	exit(ret);
}
