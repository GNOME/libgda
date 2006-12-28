#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <glib.h>

#include "mem.h"

#ifdef MEM_TEST

#define MEM_DONT_FREE

#define MAGIC_PRE_HASH  0xdeadbeef
#define MAGIC_POST_HASH 0xabcdef12

#define memsql_error(fmt,args...) fprintf (stderr, "MEM: " fmt "\n" , ##args)
#define min(a,b) (((a) > (b)) ? (b) : (a))

typedef struct
{
	int pre_id;
	char name[64];
	int size;
	int line;
	char file[32];
#ifdef MEM_DONT_FREE

	int freed;
#endif
}
memsql_pre_info;

typedef struct
{
	int post_id;
}
memsql_post_info;

static GList *memsql_list = NULL;

static void *
memsql_get_post (memsql_pre_info * pre)
{
	return ((char *) pre) + sizeof (memsql_pre_info) + pre->size;
}

static void *
memsql_get_data (memsql_pre_info * pre)
{
	return ((char *) pre) + sizeof (memsql_pre_info);
}

static memsql_pre_info *
memsql_get_pre (void *data)
{
	return (void *) (((char *) data) - sizeof (memsql_pre_info));
}

char *
memsql_strdup_raw (char *funcname, int linenum, char *file, char *string)
{
	char *data;

	if (!string)
		return NULL;

	data = memsql_alloc_raw (funcname, linenum, file, strlen (string) + 1);
	if (data)
		strcpy (data, string);

	return data;
}

void *
memsql_strdup_printf_raw (const char *funcname, int linenum, const char *file,
			  const char *fmt, ...)
{
	void *retptr;
	gchar *tmpstr;
	va_list args;
	va_start (args, fmt);
	tmpstr = g_strdup_vprintf (fmt, args);
	retptr = memsql_strdup_raw (funcname, linenum, file, tmpstr);
	g_free (tmpstr);
	return retptr;
}

void *
memsql_alloc_raw (const char *funcname, int linenum, const char *file, int count)
{
	int real_size;
	memsql_pre_info *pre;
	memsql_post_info *post;

	real_size = count + sizeof (memsql_pre_info) + sizeof (memsql_post_info);

	pre = malloc (real_size);

	if (!pre) {
		memsql_error ("Memory full");
		return NULL;
	}

	strncpy (pre->name, funcname, 63);
	pre->name[63] = '\0';
	strncpy (pre->file, file, 31);
	pre->file[31] = '\0';
	pre->line = linenum;
	pre->size = count;
#ifdef MEM_DONT_FREE

	pre->freed = 0;
#endif

	post = memsql_get_post (pre);

	pre->pre_id = MAGIC_PRE_HASH;
	post->post_id = MAGIC_POST_HASH;

	memsql_list = g_list_append (memsql_list, pre);

	return memsql_get_data (pre);
}

void *
memsql_calloc_raw (const char *funcname, int linenum, const char *file, int count)
{
	void *ptr;

	ptr = memsql_alloc_raw (funcname, linenum, file, count);
	if (ptr)
		memset (ptr, 0, count);

	return ptr;
}

void *
memsql_realloc_raw (const char *funcname, int linenum, const char *file, void *mem,
		    int count)
{
	void *new_mem;
	memsql_pre_info *pre;

	new_mem = memsql_alloc_raw (funcname, linenum, file, count);
	pre = memsql_get_pre (new_mem);
	if (mem != NULL)
		memcpy (new_mem, mem, min (count, pre->size));

	memsql_free (mem);

	return new_mem;
}

int
memsql_free (void *mem)
{
	memsql_pre_info *pre;
	memsql_post_info *post;

	if (!mem)
		return 0;

	pre = memsql_get_pre (mem);

	if (pre->pre_id != MAGIC_PRE_HASH) {
		memsql_error ("pre hash doesn't match on: %s", pre->name);
		g_assert (NULL);
	}
	post = memsql_get_post (pre);
	if (post->post_id != MAGIC_POST_HASH) {
		memsql_error ("post hash doesn't match on: %s", pre->name);
		g_assert (NULL);
	}

	/*   memsql_error ("mem list length: %d", g_list_length (memsql_list)); */

#ifdef MEM_DONT_FREE
	if (pre->freed)
		memsql_error ("freeing memory block `%s' for a second time", pre->name);

	pre->freed = 1;
#else

	memsql_list = g_list_remove (memsql_list, pre);
	free (pre);
#endif

	return 0;
}

int
memsql_display (void)
{
	void *buf;
	GList *cur;
	memsql_pre_info *pre;
	int count = 0;
	int bytes = 0, i;

	for (cur = memsql_list; cur != NULL; cur = cur->next) {
		pre = cur->data;
#ifdef MEM_DONT_FREE

		if (!pre->freed) {
#endif
			memsql_error ("%s: %d: %s() %dbytes still alloced", pre->file,
				      pre->line, pre->name, pre->size);
			/* Uncomment this if you want to display the memory buffer */
			buf = memsql_get_data (pre);
			if (pre->size > 256);
			else {
				for (i = 0; i < pre->size; i++) {
					if (*((char *) buf) >= ' ' && *((char *) buf) <= 'z')
						fprintf (stderr, "%c", *((char *) buf));
					((char *) buf)++;
				}
				fprintf (stderr, "\n");
			}
			count++;
			bytes += pre->size;
#ifdef MEM_DONT_FREE

		}
#endif

	}

	/*if (count) */
	memsql_error ("%d memory leaks, totaling %d bytes", count, bytes);

	return 0;
}
#endif

char *
memsql_strappend_raw (const char *funcname, int linenum, const char *file, const char *string1,
		      const char *string2)
{
	int len = 0;
	char *retval;

	/*   memsql_error ("appending: %s, %s", string1, string2); */

	if (!string1 && !string2)
		return NULL;

	if (string1)
		len += strlen (string1);
	if (string2)
		len += strlen (string2);
#ifdef MEM_TEST
	retval = memsql_alloc_raw (funcname, linenum, file, len + 1);
#else
	retval = g_malloc (len + 1);
#endif
	retval[0] = '\0';

	if (string1)
		strcpy (retval, string1);
	if (string2)
		strcat (retval, string2);

	retval[len] = '\0';

	return retval;
}

char *
memsql_strappend_free_raw (const char *funcname, int linenum, const char *file,
			   char *str1, char *str2)
{
	char *retval;

	retval = memsql_strappend_raw (funcname, linenum, file, str1, str2);

	if (str1)
		memsql_free (str1);
	if (str2)
		memsql_free (str2);

	return retval;
}
