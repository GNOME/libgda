#ifndef _COMMON_H_
#define _COMMON_H_

#define	DATABASE "access.db"
#define COLORSIZE 10
#define NAMESIZE 10

typedef struct {
	char  color[COLORSIZE];
	int   type;
} Key;

typedef struct {
	float  size;
	char   name[NAMESIZE];
} Value;

#endif
