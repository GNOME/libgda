#ifndef MEM_H
#define MEM_H

#define GET_FUNC_DETAILS __FUNCTION__, __LINE__, __FILE__

#ifdef MEM_TEST

#define memsql_strdup_printf(fmt,args...) memsql_strdup_printf_raw(GET_FUNC_DETAILS,fmt,##args)
#define memsql_strdup(a) memsql_strdup_raw(GET_FUNC_DETAILS, a)
#define memsql_alloc(a) memsql_alloc_raw(GET_FUNC_DETAILS, a)
#define memsql_calloc(a) memsql_calloc_raw(GET_FUNC_DETAILS, a)
#define memsql_realloc(a,b) memsql_realloc_raw(GET_FUNC_DETAILS, a, b)
#define memsql_strappend(a,b) memsql_strappend_raw (GET_FUNC_DETAILS, a, b)
#define memsql_strappend_free(a,b) memsql_strappend_free_raw (GET_FUNC_DETAILS, a, b)

char *memsql_strdup_raw (char *funcname, int linenum, char *file,
			 char *string);
void *memsql_alloc_raw (char *funcname, int linenum, char *file, int count);
void *memsql_calloc_raw (char *funcname, int linenum, char *file, int count);
void *memsql_realloc_raw (char *funcname, int linenum, char *file, void *mem,
			  int count);
void *memsql_strdup_printf_raw (char *funcname, int linenum, char *file,
				const char *fmt, ...);

int memsql_free (void *mem);
int memsql_display (void);

#else

#define memsql_strdup_printf(fmt,args...) g_strdup_printf(fmt,##args)
#define memsql_strdup(a) g_strdup(a)
#define memsql_alloc(a) g_malloc(a)
#define memsql_calloc(a) g_malloc0(a)
#define memsql_realloc(a,b) g_realloc(a, b)
#define memsql_free(a) g_free(a)

#define memsql_display() printf("")

#define memsql_strappend(a,b) memsql_strappend_raw (GET_FUNC_DETAILS, a, b)
#define memsql_strappend_free(a,b) memsql_strappend_free_raw (GET_FUNC_DETAILS, a, b)

#endif

char *memsql_strappend_raw (char *funcname, int linenum, char *file,
			    char *string1, char *string2);
char *memsql_strappend_free_raw (char *funcname, int linenum, char *file,
				 char *string1, char *string2);

#endif /* MEM_H */
