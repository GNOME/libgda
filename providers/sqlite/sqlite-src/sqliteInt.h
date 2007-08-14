/************** Begin file sqliteInt.h ***************************************/
/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Internal interface definitions for SQLite.
**
** @(#) $Id$
*/
#ifndef _SQLITEINT_H_
#define _SQLITEINT_H_
/************** Include sqliteLimit.h in the middle of sqliteInt.h ***********/
/************** Begin file sqliteLimit.h *************************************/
/*
** 2007 May 7
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** 
** This file defines various limits of what SQLite can process.
**
** @(#) $Id$
*/

/*
** The maximum length of a TEXT or BLOB in bytes.   This also
** limits the size of a row in a table or index.
**
** The hard limit is the ability of a 32-bit signed integer
** to count the size: 2^31-1 or 2147483647.
*/
#ifndef SQLITE_MAX_LENGTH
# define SQLITE_MAX_LENGTH 1000000000
#endif

/*
** This is the maximum number of
**
**    * Columns in a table
**    * Columns in an index
**    * Columns in a view
**    * Terms in the SET clause of an UPDATE statement
**    * Terms in the result set of a SELECT statement
**    * Terms in the GROUP BY or ORDER BY clauses of a SELECT statement.
**    * Terms in the VALUES clause of an INSERT statement
**
** The hard upper limit here is 32676.  Most database people will
** tell you that in a well-normalized database, you usually should
** not have more than a dozen or so columns in any table.  And if
** that is the case, there is no point in having more than a few
** dozen values in any of the other situations described above.
*/
#ifndef SQLITE_MAX_COLUMN
# define SQLITE_MAX_COLUMN 2000
#endif

/*
** The maximum length of a single SQL statement in bytes.
** The hard limit here is the same as SQLITE_MAX_LENGTH.
*/
#ifndef SQLITE_MAX_SQL_LENGTH
# define SQLITE_MAX_SQL_LENGTH 1000000
#endif

/*
** The maximum depth of an expression tree. This is limited to 
** some extent by SQLITE_MAX_SQL_LENGTH. But sometime you might 
** want to place more severe limits on the complexity of an 
** expression. A value of 0 (the default) means do not enforce
** any limitation on expression tree depth.
*/
#ifndef SQLITE_MAX_EXPR_DEPTH
# define SQLITE_MAX_EXPR_DEPTH 1000
#endif

/*
** The maximum number of terms in a compound SELECT statement.
** The code generator for compound SELECT statements does one
** level of recursion for each term.  A stack overflow can result
** if the number of terms is too large.  In practice, most SQL
** never has more than 3 or 4 terms.  Use a value of 0 to disable
** any limit on the number of terms in a compount SELECT.
*/
#ifndef SQLITE_MAX_COMPOUND_SELECT
# define SQLITE_MAX_COMPOUND_SELECT 500
#endif

/*
** The maximum number of opcodes in a VDBE program.
** Not currently enforced.
*/
#ifndef SQLITE_MAX_VDBE_OP
# define SQLITE_MAX_VDBE_OP 25000
#endif

/*
** The maximum number of arguments to an SQL function.
*/
#ifndef SQLITE_MAX_FUNCTION_ARG
# define SQLITE_MAX_FUNCTION_ARG 100
#endif

/*
** The maximum number of in-memory pages to use for the main database
** table and for temporary tables.  The SQLITE_DEFAULT_CACHE_SIZE
*/
#ifndef SQLITE_DEFAULT_CACHE_SIZE
# define SQLITE_DEFAULT_CACHE_SIZE  2000
#endif
#ifndef SQLITE_DEFAULT_TEMP_CACHE_SIZE
# define SQLITE_DEFAULT_TEMP_CACHE_SIZE  500
#endif

/*
** The maximum number of attached databases.  This must be at least 2
** in order to support the main database file (0) and the file used to
** hold temporary tables (1).  And it must be less than 32 because
** we use a bitmask of databases with a u32 in places (for example
** the Parse.cookieMask field).
*/
#ifndef SQLITE_MAX_ATTACHED
# define SQLITE_MAX_ATTACHED 10
#endif


/*
** The maximum value of a ?nnn wildcard that the parser will accept.
*/
#ifndef SQLITE_MAX_VARIABLE_NUMBER
# define SQLITE_MAX_VARIABLE_NUMBER 999
#endif

/*
** The default size of a database page.
*/
#ifndef SQLITE_DEFAULT_PAGE_SIZE
# define SQLITE_DEFAULT_PAGE_SIZE 1024
#endif

/* Maximum page size.  The upper bound on this value is 32768.  This a limit
** imposed by the necessity of storing the value in a 2-byte unsigned integer
** and the fact that the page size must be a power of 2.
*/
#ifndef SQLITE_MAX_PAGE_SIZE
# define SQLITE_MAX_PAGE_SIZE 32768
#endif

/*
** Maximum number of pages in one database file.
**
** This is really just the default value for the max_page_count pragma.
** This value can be lowered (or raised) at run-time using that the
** max_page_count macro.
*/
#ifndef SQLITE_MAX_PAGE_COUNT
# define SQLITE_MAX_PAGE_COUNT 1073741823
#endif

/*
** Maximum length (in bytes) of the pattern in a LIKE or GLOB
** operator.
*/
#ifndef SQLITE_MAX_LIKE_PATTERN_LENGTH
# define SQLITE_MAX_LIKE_PATTERN_LENGTH 50000
#endif

/************** End of sqliteLimit.h *****************************************/
/************** Continuing where we left off in sqliteInt.h ******************/


#if defined(SQLITE_TCL) || defined(TCLSH)
# include <tcl.h>
#endif

/*
** Many people are failing to set -DNDEBUG=1 when compiling SQLite.
** Setting NDEBUG makes the code smaller and run faster.  So the following
** lines are added to automatically set NDEBUG unless the -DSQLITE_DEBUG=1
** option is set.  Thus NDEBUG becomes an opt-in rather than an opt-out
** feature.
*/
#if !defined(NDEBUG) && !defined(SQLITE_DEBUG) 
# define NDEBUG 1
#endif

/*
** These #defines should enable >2GB file support on Posix if the
** underlying operating system supports it.  If the OS lacks
** large file support, or if the OS is windows, these should be no-ops.
**
** Large file support can be disabled using the -DSQLITE_DISABLE_LFS switch
** on the compiler command line.  This is necessary if you are compiling
** on a recent machine (ex: RedHat 7.2) but you want your code to work
** on an older machine (ex: RedHat 6.0).  If you compile on RedHat 7.2
** without this option, LFS is enable.  But LFS does not exist in the kernel
** in RedHat 6.0, so the code won't work.  Hence, for maximum binary
** portability you should omit LFS.
**
** Similar is true for MacOS.  LFS is only supported on MacOS 9 and later.
*/
#ifndef SQLITE_DISABLE_LFS
# define _LARGE_FILE       1
# ifndef _FILE_OFFSET_BITS
#   define _FILE_OFFSET_BITS 64
# endif
# define _LARGEFILE_SOURCE 1
#endif

/************** Include hash.h in the middle of sqliteInt.h ******************/
/************** Begin file hash.h ********************************************/
/*
** 2001 September 22
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This is the header file for the generic hash-table implemenation
** used in SQLite.
**
** $Id$
*/
#ifndef _SQLITE_HASH_H_
#define _SQLITE_HASH_H_

/* Forward declarations of structures. */
typedef struct Hash Hash;
typedef struct HashElem HashElem;

/* A complete hash table is an instance of the following structure.
** The internals of this structure are intended to be opaque -- client
** code should not attempt to access or modify the fields of this structure
** directly.  Change this structure only by using the routines below.
** However, many of the "procedures" and "functions" for modifying and
** accessing this structure are really macros, so we can't really make
** this structure opaque.
*/
struct Hash {
  char keyClass;          /* SQLITE_HASH_INT, _POINTER, _STRING, _BINARY */
  char copyKey;           /* True if copy of key made on insert */
  int count;              /* Number of entries in this table */
  HashElem *first;        /* The first element of the array */
  void *(*xMalloc)(int);  /* malloc() function to use */
  void (*xFree)(void *);  /* free() function to use */
  int htsize;             /* Number of buckets in the hash table */
  struct _ht {            /* the hash table */
    int count;               /* Number of entries with this hash */
    HashElem *chain;         /* Pointer to first entry with this hash */
  } *ht;
};

/* Each element in the hash table is an instance of the following 
** structure.  All elements are stored on a single doubly-linked list.
**
** Again, this structure is intended to be opaque, but it can't really
** be opaque because it is used by macros.
*/
struct HashElem {
  HashElem *next, *prev;   /* Next and previous elements in the table */
  void *data;              /* Data associated with this element */
  void *pKey; int nKey;    /* Key associated with this element */
};

/*
** There are 4 different modes of operation for a hash table:
**
**   SQLITE_HASH_INT         nKey is used as the key and pKey is ignored.
**
**   SQLITE_HASH_POINTER     pKey is used as the key and nKey is ignored.
**
**   SQLITE_HASH_STRING      pKey points to a string that is nKey bytes long
**                           (including the null-terminator, if any).  Case
**                           is ignored in comparisons.
**
**   SQLITE_HASH_BINARY      pKey points to binary data nKey bytes long. 
**                           memcmp() is used to compare keys.
**
** A copy of the key is made for SQLITE_HASH_STRING and SQLITE_HASH_BINARY
** if the copyKey parameter to HashInit is 1.  
*/
/* #define SQLITE_HASH_INT       1 // NOT USED */
/* #define SQLITE_HASH_POINTER   2 // NOT USED */
#define SQLITE_HASH_STRING    3
#define SQLITE_HASH_BINARY    4

/*
** Access routines.  To delete, insert a NULL pointer.
*/
SQLITE_PRIVATE void sqlite3HashInit(Hash*, int keytype, int copyKey);
SQLITE_PRIVATE void *sqlite3HashInsert(Hash*, const void *pKey, int nKey, void *pData);
SQLITE_PRIVATE void *sqlite3HashFind(const Hash*, const void *pKey, int nKey);
SQLITE_PRIVATE void sqlite3HashClear(Hash*);

/*
** Macros for looping over all elements of a hash table.  The idiom is
** like this:
**
**   Hash h;
**   HashElem *p;
**   ...
**   for(p=sqliteHashFirst(&h); p; p=sqliteHashNext(p)){
**     SomeStructure *pData = sqliteHashData(p);
**     // do something with pData
**   }
*/
#define sqliteHashFirst(H)  ((H)->first)
#define sqliteHashNext(E)   ((E)->next)
#define sqliteHashData(E)   ((E)->data)
#define sqliteHashKey(E)    ((E)->pKey)
#define sqliteHashKeysize(E) ((E)->nKey)

/*
** Number of entries in a hash table
*/
#define sqliteHashCount(H)  ((H)->count)

#endif /* _SQLITE_HASH_H_ */

/************** End of hash.h ************************************************/
/************** Continuing where we left off in sqliteInt.h ******************/
/************** Include parse.h in the middle of sqliteInt.h *****************/
/************** Begin file parse.h *******************************************/
#define TK_SEMI                            1
#define TK_EXPLAIN                         2
#define TK_QUERY                           3
#define TK_PLAN                            4
#define TK_BEGIN                           5
#define TK_TRANSACTION                     6
#define TK_DEFERRED                        7
#define TK_IMMEDIATE                       8
#define TK_EXCLUSIVE                       9
#define TK_COMMIT                         10
#define TK_END                            11
#define TK_ROLLBACK                       12
#define TK_CREATE                         13
#define TK_TABLE                          14
#define TK_IF                             15
#define TK_NOT                            16
#define TK_EXISTS                         17
#define TK_TEMP                           18
#define TK_LP                             19
#define TK_RP                             20
#define TK_AS                             21
#define TK_COMMA                          22
#define TK_ID                             23
#define TK_ABORT                          24
#define TK_AFTER                          25
#define TK_ANALYZE                        26
#define TK_ASC                            27
#define TK_ATTACH                         28
#define TK_BEFORE                         29
#define TK_CASCADE                        30
#define TK_CAST                           31
#define TK_CONFLICT                       32
#define TK_DATABASE                       33
#define TK_DESC                           34
#define TK_DETACH                         35
#define TK_EACH                           36
#define TK_FAIL                           37
#define TK_FOR                            38
#define TK_IGNORE                         39
#define TK_INITIALLY                      40
#define TK_INSTEAD                        41
#define TK_LIKE_KW                        42
#define TK_MATCH                          43
#define TK_KEY                            44
#define TK_OF                             45
#define TK_OFFSET                         46
#define TK_PRAGMA                         47
#define TK_RAISE                          48
#define TK_REPLACE                        49
#define TK_RESTRICT                       50
#define TK_ROW                            51
#define TK_TRIGGER                        52
#define TK_VACUUM                         53
#define TK_VIEW                           54
#define TK_VIRTUAL                        55
#define TK_REINDEX                        56
#define TK_RENAME                         57
#define TK_CTIME_KW                       58
#define TK_ANY                            59
#define TK_OR                             60
#define TK_AND                            61
#define TK_IS                             62
#define TK_BETWEEN                        63
#define TK_IN                             64
#define TK_ISNULL                         65
#define TK_NOTNULL                        66
#define TK_NE                             67
#define TK_EQ                             68
#define TK_GT                             69
#define TK_LE                             70
#define TK_LT                             71
#define TK_GE                             72
#define TK_ESCAPE                         73
#define TK_BITAND                         74
#define TK_BITOR                          75
#define TK_LSHIFT                         76
#define TK_RSHIFT                         77
#define TK_PLUS                           78
#define TK_MINUS                          79
#define TK_STAR                           80
#define TK_SLASH                          81
#define TK_REM                            82
#define TK_CONCAT                         83
#define TK_COLLATE                        84
#define TK_UMINUS                         85
#define TK_UPLUS                          86
#define TK_BITNOT                         87
#define TK_STRING                         88
#define TK_JOIN_KW                        89
#define TK_CONSTRAINT                     90
#define TK_DEFAULT                        91
#define TK_NULL                           92
#define TK_PRIMARY                        93
#define TK_UNIQUE                         94
#define TK_CHECK                          95
#define TK_REFERENCES                     96
#define TK_AUTOINCR                       97
#define TK_ON                             98
#define TK_DELETE                         99
#define TK_UPDATE                         100
#define TK_INSERT                         101
#define TK_SET                            102
#define TK_DEFERRABLE                     103
#define TK_FOREIGN                        104
#define TK_DROP                           105
#define TK_UNION                          106
#define TK_ALL                            107
#define TK_EXCEPT                         108
#define TK_INTERSECT                      109
#define TK_SELECT                         110
#define TK_DISTINCT                       111
#define TK_DOT                            112
#define TK_FROM                           113
#define TK_JOIN                           114
#define TK_USING                          115
#define TK_ORDER                          116
#define TK_BY                             117
#define TK_GROUP                          118
#define TK_HAVING                         119
#define TK_LIMIT                          120
#define TK_WHERE                          121
#define TK_INTO                           122
#define TK_VALUES                         123
#define TK_INTEGER                        124
#define TK_FLOAT                          125
#define TK_BLOB                           126
#define TK_REGISTER                       127
#define TK_VARIABLE                       128
#define TK_CASE                           129
#define TK_WHEN                           130
#define TK_THEN                           131
#define TK_ELSE                           132
#define TK_INDEX                          133
#define TK_ALTER                          134
#define TK_TO                             135
#define TK_ADD                            136
#define TK_COLUMNKW                       137
#define TK_TO_TEXT                        138
#define TK_TO_BLOB                        139
#define TK_TO_NUMERIC                     140
#define TK_TO_INT                         141
#define TK_TO_REAL                        142
#define TK_END_OF_FILE                    143
#define TK_ILLEGAL                        144
#define TK_SPACE                          145
#define TK_UNCLOSED_STRING                146
#define TK_COMMENT                        147
#define TK_FUNCTION                       148
#define TK_COLUMN                         149
#define TK_AGG_FUNCTION                   150
#define TK_AGG_COLUMN                     151
#define TK_CONST_FUNC                     152

/************** End of parse.h ***********************************************/
/************** Continuing where we left off in sqliteInt.h ******************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

#define sqlite3_isnan(X)  ((X)!=(X))

/*
** If compiling for a processor that lacks floating point support,
** substitute integer for floating-point
*/
#ifdef SQLITE_OMIT_FLOATING_POINT
# define double sqlite_int64
# define LONGDOUBLE_TYPE sqlite_int64
# ifndef SQLITE_BIG_DBL
#   define SQLITE_BIG_DBL (0x7fffffffffffffff)
# endif
# define SQLITE_OMIT_DATETIME_FUNCS 1
# define SQLITE_OMIT_TRACE 1
# undef SQLITE_MIXED_ENDIAN_64BIT_FLOAT
#endif
#ifndef SQLITE_BIG_DBL
# define SQLITE_BIG_DBL (1e99)
#endif

/*
** OMIT_TEMPDB is set to 1 if SQLITE_OMIT_TEMPDB is defined, or 0
** afterward. Having this macro allows us to cause the C compiler 
** to omit code used by TEMP tables without messy #ifndef statements.
*/
#ifdef SQLITE_OMIT_TEMPDB
#define OMIT_TEMPDB 1
#else
#define OMIT_TEMPDB 0
#endif

/*
** If the following macro is set to 1, then NULL values are considered
** distinct when determining whether or not two entries are the same
** in a UNIQUE index.  This is the way PostgreSQL, Oracle, DB2, MySQL,
** OCELOT, and Firebird all work.  The SQL92 spec explicitly says this
** is the way things are suppose to work.
**
** If the following macro is set to 0, the NULLs are indistinct for
** a UNIQUE index.  In this mode, you can only have a single NULL entry
** for a column declared UNIQUE.  This is the way Informix and SQL Server
** work.
*/
#define NULL_DISTINCT_FOR_UNIQUE 1

/*
** The "file format" number is an integer that is incremented whenever
** the VDBE-level file format changes.  The following macros define the
** the default file format for new databases and the maximum file format
** that the library can read.
*/
#define SQLITE_MAX_FILE_FORMAT 4
#ifndef SQLITE_DEFAULT_FILE_FORMAT
# define SQLITE_DEFAULT_FILE_FORMAT 1
#endif

/*
** Provide a default value for TEMP_STORE in case it is not specified
** on the command-line
*/
#ifndef TEMP_STORE
# define TEMP_STORE 1
#endif

/*
** GCC does not define the offsetof() macro so we'll have to do it
** ourselves.
*/
#ifndef offsetof
#define offsetof(STRUCTURE,FIELD) ((int)((char*)&((STRUCTURE*)0)->FIELD))
#endif

/*
** Check to see if this machine uses EBCDIC.  (Yes, believe it or
** not, there are still machines out there that use EBCDIC.)
*/
#if 'A' == '\301'
# define SQLITE_EBCDIC 1
#else
# define SQLITE_ASCII 1
#endif

/*
** Integers of known sizes.  These typedefs might change for architectures
** where the sizes very.  Preprocessor macros are available so that the
** types can be conveniently redefined at compile-type.  Like this:
**
**         cc '-DUINTPTR_TYPE=long long int' ...
*/
#ifndef UINT32_TYPE
# define UINT32_TYPE unsigned int
#endif
#ifndef UINT16_TYPE
# define UINT16_TYPE unsigned short int
#endif
#ifndef INT16_TYPE
# define INT16_TYPE short int
#endif
#ifndef UINT8_TYPE
# define UINT8_TYPE unsigned char
#endif
#ifndef INT8_TYPE
# define INT8_TYPE signed char
#endif
#ifndef LONGDOUBLE_TYPE
# define LONGDOUBLE_TYPE long double
#endif
typedef sqlite_int64 i64;          /* 8-byte signed integer */
typedef sqlite_uint64 u64;         /* 8-byte unsigned integer */
typedef UINT32_TYPE u32;           /* 4-byte unsigned integer */
typedef UINT16_TYPE u16;           /* 2-byte unsigned integer */
typedef INT16_TYPE i16;            /* 2-byte signed integer */
typedef UINT8_TYPE u8;             /* 1-byte unsigned integer */
typedef UINT8_TYPE i8;             /* 1-byte signed integer */

/*
** Macros to determine whether the machine is big or little endian,
** evaluated at runtime.
*/
SQLITE_PRIVATE const int sqlite3one;
#if defined(i386) || defined(__i386__) || defined(_M_IX86)
# define SQLITE_BIGENDIAN    0
# define SQLITE_LITTLEENDIAN 1
# define SQLITE_UTF16NATIVE  SQLITE_UTF16LE
#else
# define SQLITE_BIGENDIAN    (*(char *)(&sqlite3one)==0)
# define SQLITE_LITTLEENDIAN (*(char *)(&sqlite3one)==1)
# define SQLITE_UTF16NATIVE (SQLITE_BIGENDIAN?SQLITE_UTF16BE:SQLITE_UTF16LE)
#endif

/*
** An instance of the following structure is used to store the busy-handler
** callback for a given sqlite handle. 
**
** The sqlite.busyHandler member of the sqlite struct contains the busy
** callback for the database handle. Each pager opened via the sqlite
** handle is passed a pointer to sqlite.busyHandler. The busy-handler
** callback is currently invoked only from within pager.c.
*/
typedef struct BusyHandler BusyHandler;
struct BusyHandler {
  int (*xFunc)(void *,int);  /* The busy callback */
  void *pArg;                /* First arg to busy callback */
  int nBusy;                 /* Incremented with each busy call */
};

/*
** Defer sourcing vdbe.h and btree.h until after the "u8" and 
** "BusyHandler typedefs.
*/
/************** Include vdbe.h in the middle of sqliteInt.h ******************/
/************** Begin file vdbe.h ********************************************/
/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Header file for the Virtual DataBase Engine (VDBE)
**
** This header defines the interface to the virtual database engine
** or VDBE.  The VDBE implements an abstract machine that runs a
** simple program to access and modify the underlying database.
**
** $Id$
*/
#ifndef _SQLITE_VDBE_H_
#define _SQLITE_VDBE_H_

/*
** A single VDBE is an opaque structure named "Vdbe".  Only routines
** in the source file sqliteVdbe.c are allowed to see the insides
** of this structure.
*/
typedef struct Vdbe Vdbe;

/*
** A single instruction of the virtual machine has an opcode
** and as many as three operands.  The instruction is recorded
** as an instance of the following structure:
*/
struct VdbeOp {
  u8 opcode;          /* What operation to perform */
  int p1;             /* First operand */
  int p2;             /* Second parameter (often the jump destination) */
  char *p3;           /* Third parameter */
  int p3type;         /* One of the P3_xxx constants defined below */
#ifdef VDBE_PROFILE
  int cnt;            /* Number of times this instruction was executed */
  long long cycles;   /* Total time spend executing this instruction */
#endif
};
typedef struct VdbeOp VdbeOp;

/*
** A smaller version of VdbeOp used for the VdbeAddOpList() function because
** it takes up less space.
*/
struct VdbeOpList {
  u8 opcode;          /* What operation to perform */
  signed char p1;     /* First operand */
  short int p2;       /* Second parameter (often the jump destination) */
  char *p3;           /* Third parameter */
};
typedef struct VdbeOpList VdbeOpList;

/*
** Allowed values of VdbeOp.p3type
*/
#define P3_NOTUSED    0   /* The P3 parameter is not used */
#define P3_DYNAMIC  (-1)  /* Pointer to a string obtained from sqliteMalloc() */
#define P3_STATIC   (-2)  /* Pointer to a static string */
#define P3_COLLSEQ  (-4)  /* P3 is a pointer to a CollSeq structure */
#define P3_FUNCDEF  (-5)  /* P3 is a pointer to a FuncDef structure */
#define P3_KEYINFO  (-6)  /* P3 is a pointer to a KeyInfo structure */
#define P3_VDBEFUNC (-7)  /* P3 is a pointer to a VdbeFunc structure */
#define P3_MEM      (-8)  /* P3 is a pointer to a Mem*    structure */
#define P3_TRANSIENT (-9) /* P3 is a pointer to a transient string */
#define P3_VTAB     (-10) /* P3 is a pointer to an sqlite3_vtab structure */
#define P3_MPRINTF  (-11) /* P3 is a string obtained from sqlite3_mprintf() */

/* When adding a P3 argument using P3_KEYINFO, a copy of the KeyInfo structure
** is made.  That copy is freed when the Vdbe is finalized.  But if the
** argument is P3_KEYINFO_HANDOFF, the passed in pointer is used.  It still
** gets freed when the Vdbe is finalized so it still should be obtained
** from a single sqliteMalloc().  But no copy is made and the calling
** function should *not* try to free the KeyInfo.
*/
#define P3_KEYINFO_HANDOFF (-9)

/*
** The Vdbe.aColName array contains 5n Mem structures, where n is the 
** number of columns of data returned by the statement.
*/
#define COLNAME_NAME     0
#define COLNAME_DECLTYPE 1
#define COLNAME_DATABASE 2
#define COLNAME_TABLE    3
#define COLNAME_COLUMN   4
#define COLNAME_N        5      /* Number of COLNAME_xxx symbols */

/*
** The following macro converts a relative address in the p2 field
** of a VdbeOp structure into a negative number so that 
** sqlite3VdbeAddOpList() knows that the address is relative.  Calling
** the macro again restores the address.
*/
#define ADDR(X)  (-1-(X))

/*
** The makefile scans the vdbe.c source file and creates the "opcodes.h"
** header file that defines a number for each opcode used by the VDBE.
*/
/************** Include opcodes.h in the middle of vdbe.h ********************/
/************** Begin file opcodes.h *****************************************/
/* Automatically generated.  Do not edit */
/* See the mkopcodeh.awk script for details */
#define OP_MemLoad                              1
#define OP_VNext                                2
#define OP_HexBlob                            126   /* same as TK_BLOB     */
#define OP_Column                               3
#define OP_SetCookie                            4
#define OP_IfMemPos                             5
#define OP_Real                               125   /* same as TK_FLOAT    */
#define OP_Sequence                             6
#define OP_MoveGt                               7
#define OP_Ge                                  72   /* same as TK_GE       */
#define OP_RowKey                               8
#define OP_Eq                                  68   /* same as TK_EQ       */
#define OP_OpenWrite                            9
#define OP_NotNull                             66   /* same as TK_NOTNULL  */
#define OP_If                                  10
#define OP_ToInt                              141   /* same as TK_TO_INT   */
#define OP_String8                             88   /* same as TK_STRING   */
#define OP_Pop                                 11
#define OP_VRowid                              12
#define OP_CollSeq                             13
#define OP_OpenRead                            14
#define OP_Expire                              15
#define OP_AutoCommit                          17
#define OP_Gt                                  69   /* same as TK_GT       */
#define OP_IntegrityCk                         18
#define OP_Sort                                19
#define OP_Function                            20
#define OP_And                                 61   /* same as TK_AND      */
#define OP_Subtract                            79   /* same as TK_MINUS    */
#define OP_Noop                                21
#define OP_Return                              22
#define OP_Remainder                           82   /* same as TK_REM      */
#define OP_NewRowid                            23
#define OP_Multiply                            80   /* same as TK_STAR     */
#define OP_IfMemNeg                            24
#define OP_Variable                            25
#define OP_String                              26
#define OP_RealAffinity                        27
#define OP_VRename                             28
#define OP_ParseSchema                         29
#define OP_VOpen                               30
#define OP_Close                               31
#define OP_CreateIndex                         32
#define OP_IsUnique                            33
#define OP_NotFound                            34
#define OP_Int64                               35
#define OP_MustBeInt                           36
#define OP_Halt                                37
#define OP_Rowid                               38
#define OP_IdxLT                               39
#define OP_AddImm                              40
#define OP_Statement                           41
#define OP_RowData                             42
#define OP_MemMax                              43
#define OP_Push                                44
#define OP_Or                                  60   /* same as TK_OR       */
#define OP_NotExists                           45
#define OP_MemIncr                             46
#define OP_Gosub                               47
#define OP_Divide                              81   /* same as TK_SLASH    */
#define OP_Integer                             48
#define OP_ToNumeric                          140   /* same as TK_TO_NUMERIC*/
#define OP_MemInt                              49
#define OP_Prev                                50
#define OP_Concat                              83   /* same as TK_CONCAT   */
#define OP_BitAnd                              74   /* same as TK_BITAND   */
#define OP_VColumn                             51
#define OP_CreateTable                         52
#define OP_Last                                53
#define OP_IsNull                              65   /* same as TK_ISNULL   */
#define OP_IncrVacuum                          54
#define OP_IdxRowid                            55
#define OP_MakeIdxRec                          56
#define OP_ShiftRight                          77   /* same as TK_RSHIFT   */
#define OP_ResetCount                          57
#define OP_FifoWrite                           58
#define OP_Callback                            59
#define OP_ContextPush                         62
#define OP_DropTrigger                         63
#define OP_DropIndex                           64
#define OP_IdxGE                               73
#define OP_IdxDelete                           84
#define OP_Vacuum                              86
#define OP_MoveLe                              89
#define OP_IfNot                               90
#define OP_DropTable                           91
#define OP_MakeRecord                          92
#define OP_ToBlob                             139   /* same as TK_TO_BLOB  */
#define OP_Delete                              93
#define OP_AggFinal                            94
#define OP_ShiftLeft                           76   /* same as TK_LSHIFT   */
#define OP_Dup                                 95
#define OP_Goto                                96
#define OP_TableLock                           97
#define OP_FifoRead                            98
#define OP_Clear                               99
#define OP_IdxGT                              100
#define OP_MoveLt                             101
#define OP_Le                                  70   /* same as TK_LE       */
#define OP_VerifyCookie                       102
#define OP_AggStep                            103
#define OP_Pull                               104
#define OP_ToText                             138   /* same as TK_TO_TEXT  */
#define OP_Not                                 16   /* same as TK_NOT      */
#define OP_ToReal                             142   /* same as TK_TO_REAL  */
#define OP_SetNumColumns                      105
#define OP_AbsValue                           106
#define OP_Transaction                        107
#define OP_VFilter                            108
#define OP_Negative                            85   /* same as TK_UMINUS   */
#define OP_Ne                                  67   /* same as TK_NE       */
#define OP_VDestroy                           109
#define OP_ContextPop                         110
#define OP_BitOr                               75   /* same as TK_BITOR    */
#define OP_Next                               111
#define OP_IdxInsert                          112
#define OP_Distinct                           113
#define OP_Lt                                  71   /* same as TK_LT       */
#define OP_Insert                             114
#define OP_Destroy                            115
#define OP_ReadCookie                         116
#define OP_ForceInt                           117
#define OP_LoadAnalysis                       118
#define OP_Explain                            119
#define OP_IfMemZero                          120
#define OP_OpenPseudo                         121
#define OP_OpenEphemeral                      122
#define OP_Null                               123
#define OP_Blob                               124
#define OP_Add                                 78   /* same as TK_PLUS     */
#define OP_MemStore                           127
#define OP_Rewind                             128
#define OP_MoveGe                             129
#define OP_VBegin                             130
#define OP_VUpdate                            131
#define OP_BitNot                              87   /* same as TK_BITNOT   */
#define OP_VCreate                            132
#define OP_MemMove                            133
#define OP_MemNull                            134
#define OP_Found                              135
#define OP_NullRow                            136

/* The following opcode values are never used */
#define OP_NotUsed_137                        137

/* Opcodes that are guaranteed to never push a value onto the stack
** contain a 1 their corresponding position of the following mask
** set.  See the opcodeNoPush() function in vdbeaux.c  */
#define NOPUSH_MASK_0 0xeeb4
#define NOPUSH_MASK_1 0xf96b
#define NOPUSH_MASK_2 0xfbb6
#define NOPUSH_MASK_3 0xfe64
#define NOPUSH_MASK_4 0xffff
#define NOPUSH_MASK_5 0x6ef7
#define NOPUSH_MASK_6 0xfbfb
#define NOPUSH_MASK_7 0x8767
#define NOPUSH_MASK_8 0x7d9f
#define NOPUSH_MASK_9 0x0000

/************** End of opcodes.h *********************************************/
/************** Continuing where we left off in vdbe.h ***********************/

/*
** Prototypes for the VDBE interface.  See comments on the implementation
** for a description of what each of these routines does.
*/
SQLITE_PRIVATE Vdbe *sqlite3VdbeCreate(sqlite3*);
SQLITE_PRIVATE int sqlite3VdbeAddOp(Vdbe*,int,int,int);
SQLITE_PRIVATE int sqlite3VdbeOp3(Vdbe*,int,int,int,const char *zP3,int);
SQLITE_PRIVATE int sqlite3VdbeAddOpList(Vdbe*, int nOp, VdbeOpList const *aOp);
SQLITE_PRIVATE void sqlite3VdbeChangeP1(Vdbe*, int addr, int P1);
SQLITE_PRIVATE void sqlite3VdbeChangeP2(Vdbe*, int addr, int P2);
SQLITE_PRIVATE void sqlite3VdbeJumpHere(Vdbe*, int addr);
SQLITE_PRIVATE void sqlite3VdbeChangeToNoop(Vdbe*, int addr, int N);
SQLITE_PRIVATE void sqlite3VdbeChangeP3(Vdbe*, int addr, const char *zP1, int N);
SQLITE_PRIVATE VdbeOp *sqlite3VdbeGetOp(Vdbe*, int);
SQLITE_PRIVATE int sqlite3VdbeMakeLabel(Vdbe*);
SQLITE_PRIVATE void sqlite3VdbeDelete(Vdbe*);
SQLITE_PRIVATE void sqlite3VdbeMakeReady(Vdbe*,int,int,int,int);
SQLITE_PRIVATE int sqlite3VdbeFinalize(Vdbe*);
SQLITE_PRIVATE void sqlite3VdbeResolveLabel(Vdbe*, int);
SQLITE_PRIVATE int sqlite3VdbeCurrentAddr(Vdbe*);
#ifdef SQLITE_DEBUG
SQLITE_PRIVATE   void sqlite3VdbeTrace(Vdbe*,FILE*);
#endif
SQLITE_PRIVATE void sqlite3VdbeResetStepResult(Vdbe*);
SQLITE_PRIVATE int sqlite3VdbeReset(Vdbe*);
SQLITE_PRIVATE void sqlite3VdbeSetNumCols(Vdbe*,int);
SQLITE_PRIVATE int sqlite3VdbeSetColName(Vdbe*, int, int, const char *, int);
SQLITE_PRIVATE void sqlite3VdbeCountChanges(Vdbe*);
SQLITE_PRIVATE sqlite3 *sqlite3VdbeDb(Vdbe*);
SQLITE_PRIVATE void sqlite3VdbeSetSql(Vdbe*, const char *z, int n);
SQLITE_PRIVATE const char *sqlite3VdbeGetSql(Vdbe*);
SQLITE_PRIVATE void sqlite3VdbeSwap(Vdbe*,Vdbe*);

#ifndef NDEBUG
SQLITE_PRIVATE   void sqlite3VdbeComment(Vdbe*, const char*, ...);
# define VdbeComment(X)  sqlite3VdbeComment X
#else
# define VdbeComment(X)
#endif

#endif

/************** End of vdbe.h ************************************************/
/************** Continuing where we left off in sqliteInt.h ******************/
/************** Include btree.h in the middle of sqliteInt.h *****************/
/************** Begin file btree.h *******************************************/
/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This header file defines the interface that the sqlite B-Tree file
** subsystem.  See comments in the source code for a detailed description
** of what each interface routine does.
**
** @(#) $Id$
*/
#ifndef _BTREE_H_
#define _BTREE_H_

/* TODO: This definition is just included so other modules compile. It
** needs to be revisited.
*/
#define SQLITE_N_BTREE_META 10

/*
** If defined as non-zero, auto-vacuum is enabled by default. Otherwise
** it must be turned on for each database using "PRAGMA auto_vacuum = 1".
*/
#ifndef SQLITE_DEFAULT_AUTOVACUUM
  #define SQLITE_DEFAULT_AUTOVACUUM 0
#endif

#define BTREE_AUTOVACUUM_NONE 0        /* Do not do auto-vacuum */
#define BTREE_AUTOVACUUM_FULL 1        /* Do full auto-vacuum */
#define BTREE_AUTOVACUUM_INCR 2        /* Incremental vacuum */

/*
** Forward declarations of structure
*/
typedef struct Btree Btree;
typedef struct BtCursor BtCursor;
typedef struct BtShared BtShared;


SQLITE_PRIVATE int sqlite3BtreeOpen(
  const char *zFilename,   /* Name of database file to open */
  sqlite3 *db,             /* Associated database connection */
  Btree **,                /* Return open Btree* here */
  int flags                /* Flags */
);

/* The flags parameter to sqlite3BtreeOpen can be the bitwise or of the
** following values.
**
** NOTE:  These values must match the corresponding PAGER_ values in
** pager.h.
*/
#define BTREE_OMIT_JOURNAL  1  /* Do not use journal.  No argument */
#define BTREE_NO_READLOCK   2  /* Omit readlocks on readonly files */
#define BTREE_MEMORY        4  /* In-memory DB.  No argument */

SQLITE_PRIVATE int sqlite3BtreeClose(Btree*);
SQLITE_PRIVATE int sqlite3BtreeSetBusyHandler(Btree*,BusyHandler*);
SQLITE_PRIVATE int sqlite3BtreeSetCacheSize(Btree*,int);
SQLITE_PRIVATE int sqlite3BtreeSetSafetyLevel(Btree*,int,int);
SQLITE_PRIVATE int sqlite3BtreeSyncDisabled(Btree*);
SQLITE_PRIVATE int sqlite3BtreeSetPageSize(Btree*,int,int);
SQLITE_PRIVATE int sqlite3BtreeGetPageSize(Btree*);
SQLITE_PRIVATE int sqlite3BtreeMaxPageCount(Btree*,int);
SQLITE_PRIVATE int sqlite3BtreeGetReserve(Btree*);
SQLITE_PRIVATE int sqlite3BtreeSetAutoVacuum(Btree *, int);
SQLITE_PRIVATE int sqlite3BtreeGetAutoVacuum(Btree *);
SQLITE_PRIVATE int sqlite3BtreeBeginTrans(Btree*,int);
SQLITE_PRIVATE int sqlite3BtreeCommitPhaseOne(Btree*, const char *zMaster);
SQLITE_PRIVATE int sqlite3BtreeCommitPhaseTwo(Btree*);
SQLITE_PRIVATE int sqlite3BtreeCommit(Btree*);
SQLITE_PRIVATE int sqlite3BtreeRollback(Btree*);
SQLITE_PRIVATE int sqlite3BtreeBeginStmt(Btree*);
SQLITE_PRIVATE int sqlite3BtreeCommitStmt(Btree*);
SQLITE_PRIVATE int sqlite3BtreeRollbackStmt(Btree*);
SQLITE_PRIVATE int sqlite3BtreeCreateTable(Btree*, int*, int flags);
SQLITE_PRIVATE int sqlite3BtreeIsInTrans(Btree*);
SQLITE_PRIVATE int sqlite3BtreeIsInStmt(Btree*);
SQLITE_PRIVATE int sqlite3BtreeIsInReadTrans(Btree*);
SQLITE_PRIVATE void *sqlite3BtreeSchema(Btree *, int, void(*)(void *));
SQLITE_PRIVATE int sqlite3BtreeSchemaLocked(Btree *);
SQLITE_PRIVATE int sqlite3BtreeLockTable(Btree *, int, u8);

SQLITE_PRIVATE const char *sqlite3BtreeGetFilename(Btree *);
SQLITE_PRIVATE const char *sqlite3BtreeGetDirname(Btree *);
SQLITE_PRIVATE const char *sqlite3BtreeGetJournalname(Btree *);
SQLITE_PRIVATE int sqlite3BtreeCopyFile(Btree *, Btree *);

SQLITE_PRIVATE int sqlite3BtreeIncrVacuum(Btree *);

/* The flags parameter to sqlite3BtreeCreateTable can be the bitwise OR
** of the following flags:
*/
#define BTREE_INTKEY     1    /* Table has only 64-bit signed integer keys */
#define BTREE_ZERODATA   2    /* Table has keys only - no data */
#define BTREE_LEAFDATA   4    /* Data stored in leaves only.  Implies INTKEY */

SQLITE_PRIVATE int sqlite3BtreeDropTable(Btree*, int, int*);
SQLITE_PRIVATE int sqlite3BtreeClearTable(Btree*, int);
SQLITE_PRIVATE int sqlite3BtreeGetMeta(Btree*, int idx, u32 *pValue);
SQLITE_PRIVATE int sqlite3BtreeUpdateMeta(Btree*, int idx, u32 value);

SQLITE_PRIVATE int sqlite3BtreeCursor(
  Btree*,                              /* BTree containing table to open */
  int iTable,                          /* Index of root page */
  int wrFlag,                          /* 1 for writing.  0 for read-only */
  int(*)(void*,int,const void*,int,const void*),  /* Key comparison function */
  void*,                               /* First argument to compare function */
  BtCursor **ppCursor                  /* Returned cursor */
);

SQLITE_PRIVATE int sqlite3BtreeCloseCursor(BtCursor*);
SQLITE_PRIVATE int sqlite3BtreeMoveto(BtCursor*,const void *pKey,i64 nKey,int bias,int *pRes);
SQLITE_PRIVATE int sqlite3BtreeDelete(BtCursor*);
SQLITE_PRIVATE int sqlite3BtreeInsert(BtCursor*, const void *pKey, i64 nKey,
                                  const void *pData, int nData,
                                  int nZero, int bias);
SQLITE_PRIVATE int sqlite3BtreeFirst(BtCursor*, int *pRes);
SQLITE_PRIVATE int sqlite3BtreeLast(BtCursor*, int *pRes);
SQLITE_PRIVATE int sqlite3BtreeNext(BtCursor*, int *pRes);
SQLITE_PRIVATE int sqlite3BtreeEof(BtCursor*);
SQLITE_PRIVATE int sqlite3BtreeFlags(BtCursor*);
SQLITE_PRIVATE int sqlite3BtreePrevious(BtCursor*, int *pRes);
SQLITE_PRIVATE int sqlite3BtreeKeySize(BtCursor*, i64 *pSize);
SQLITE_PRIVATE int sqlite3BtreeKey(BtCursor*, u32 offset, u32 amt, void*);
SQLITE_PRIVATE const void *sqlite3BtreeKeyFetch(BtCursor*, int *pAmt);
SQLITE_PRIVATE const void *sqlite3BtreeDataFetch(BtCursor*, int *pAmt);
SQLITE_PRIVATE int sqlite3BtreeDataSize(BtCursor*, u32 *pSize);
SQLITE_PRIVATE int sqlite3BtreeData(BtCursor*, u32 offset, u32 amt, void*);

SQLITE_PRIVATE char *sqlite3BtreeIntegrityCheck(Btree*, int *aRoot, int nRoot, int, int*);
SQLITE_PRIVATE struct Pager *sqlite3BtreePager(Btree*);

SQLITE_PRIVATE int sqlite3BtreePutData(BtCursor*, u32 offset, u32 amt, void*);
SQLITE_PRIVATE void sqlite3BtreeCacheOverflow(BtCursor *);

#ifdef SQLITE_TEST
SQLITE_PRIVATE int sqlite3BtreeCursorInfo(BtCursor*, int*, int);
SQLITE_PRIVATE void sqlite3BtreeCursorList(Btree*);
SQLITE_PRIVATE int sqlite3BtreePageDump(Btree*, int, int recursive);
#endif

#endif /* _BTREE_H_ */

/************** End of btree.h ***********************************************/
/************** Continuing where we left off in sqliteInt.h ******************/
/************** Include pager.h in the middle of sqliteInt.h *****************/
/************** Begin file pager.h *******************************************/
/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This header file defines the interface that the sqlite page cache
** subsystem.  The page cache subsystem reads and writes a file a page
** at a time and provides a journal for rollback.
**
** @(#) $Id$
*/

#ifndef _PAGER_H_
#define _PAGER_H_

/*
** The type used to represent a page number.  The first page in a file
** is called page 1.  0 is used to represent "not a page".
*/
typedef unsigned int Pgno;

/*
** Each open file is managed by a separate instance of the "Pager" structure.
*/
typedef struct Pager Pager;

/*
** Handle type for pages.
*/
typedef struct PgHdr DbPage;

/*
** Allowed values for the flags parameter to sqlite3PagerOpen().
**
** NOTE: This values must match the corresponding BTREE_ values in btree.h.
*/
#define PAGER_OMIT_JOURNAL  0x0001    /* Do not use a rollback journal */
#define PAGER_NO_READLOCK   0x0002    /* Omit readlocks on readonly files */

/*
** Valid values for the second argument to sqlite3PagerLockingMode().
*/
#define PAGER_LOCKINGMODE_QUERY      -1
#define PAGER_LOCKINGMODE_NORMAL      0
#define PAGER_LOCKINGMODE_EXCLUSIVE   1

/*
** See source code comments for a detailed description of the following
** routines:
*/
SQLITE_PRIVATE int sqlite3PagerOpen(Pager **ppPager, const char *zFilename,
                     int nExtra, int flags);
SQLITE_PRIVATE void sqlite3PagerSetBusyhandler(Pager*, BusyHandler *pBusyHandler);
SQLITE_PRIVATE void sqlite3PagerSetDestructor(Pager*, void(*)(DbPage*,int));
SQLITE_PRIVATE void sqlite3PagerSetReiniter(Pager*, void(*)(DbPage*,int));
SQLITE_PRIVATE int sqlite3PagerSetPagesize(Pager*, int);
SQLITE_PRIVATE int sqlite3PagerMaxPageCount(Pager*, int);
SQLITE_PRIVATE int sqlite3PagerReadFileheader(Pager*, int, unsigned char*);
SQLITE_PRIVATE void sqlite3PagerSetCachesize(Pager*, int);
SQLITE_PRIVATE int sqlite3PagerClose(Pager *pPager);
SQLITE_PRIVATE int sqlite3PagerAcquire(Pager *pPager, Pgno pgno, DbPage **ppPage, int clrFlag);
#define sqlite3PagerGet(A,B,C) sqlite3PagerAcquire(A,B,C,0)
SQLITE_PRIVATE DbPage *sqlite3PagerLookup(Pager *pPager, Pgno pgno);
SQLITE_PRIVATE int sqlite3PagerRef(DbPage*);
SQLITE_PRIVATE int sqlite3PagerUnref(DbPage*);
SQLITE_PRIVATE int sqlite3PagerWrite(DbPage*);
SQLITE_PRIVATE int sqlite3PagerOverwrite(Pager *pPager, Pgno pgno, void*);
SQLITE_PRIVATE int sqlite3PagerPagecount(Pager*);
SQLITE_PRIVATE int sqlite3PagerTruncate(Pager*,Pgno);
SQLITE_PRIVATE int sqlite3PagerBegin(DbPage*, int exFlag);
SQLITE_PRIVATE int sqlite3PagerCommitPhaseOne(Pager*,const char *zMaster, Pgno);
SQLITE_PRIVATE int sqlite3PagerCommitPhaseTwo(Pager*);
SQLITE_PRIVATE int sqlite3PagerRollback(Pager*);
SQLITE_PRIVATE int sqlite3PagerIsreadonly(Pager*);
SQLITE_PRIVATE int sqlite3PagerStmtBegin(Pager*);
SQLITE_PRIVATE int sqlite3PagerStmtCommit(Pager*);
SQLITE_PRIVATE int sqlite3PagerStmtRollback(Pager*);
SQLITE_PRIVATE void sqlite3PagerDontRollback(DbPage*);
SQLITE_PRIVATE void sqlite3PagerDontWrite(DbPage*);
SQLITE_PRIVATE int sqlite3PagerRefcount(Pager*);
SQLITE_PRIVATE void sqlite3PagerSetSafetyLevel(Pager*,int,int);
SQLITE_PRIVATE const char *sqlite3PagerFilename(Pager*);
SQLITE_PRIVATE const char *sqlite3PagerDirname(Pager*);
SQLITE_PRIVATE const char *sqlite3PagerJournalname(Pager*);
SQLITE_PRIVATE int sqlite3PagerNosync(Pager*);
SQLITE_PRIVATE int sqlite3PagerMovepage(Pager*,DbPage*,Pgno);
SQLITE_PRIVATE void *sqlite3PagerGetData(DbPage *); 
SQLITE_PRIVATE void *sqlite3PagerGetExtra(DbPage *); 
SQLITE_PRIVATE int sqlite3PagerLockingMode(Pager *, int);

#if defined(SQLITE_ENABLE_MEMORY_MANAGEMENT) && !defined(SQLITE_OMIT_DISKIO)
SQLITE_PRIVATE   int sqlite3PagerReleaseMemory(int);
#endif

#ifdef SQLITE_HAS_CODEC
SQLITE_PRIVATE   void sqlite3PagerSetCodec(Pager*,void*(*)(void*,void*,Pgno,int),void*);
#endif

#if !defined(NDEBUG) || defined(SQLITE_TEST)
SQLITE_PRIVATE   Pgno sqlite3PagerPagenumber(DbPage*);
SQLITE_PRIVATE   int sqlite3PagerIswriteable(DbPage*);
#endif

#if defined(SQLITE_DEBUG) || defined(SQLITE_TEST)
SQLITE_PRIVATE   int sqlite3PagerLockstate(Pager*);
#endif

#ifdef SQLITE_TEST
SQLITE_PRIVATE   int *sqlite3PagerStats(Pager*);
SQLITE_PRIVATE   void sqlite3PagerRefdump(Pager*);
  int pager3_refinfo_enable;
#endif

#ifdef SQLITE_TEST
void disable_simulated_io_errors(void);
void enable_simulated_io_errors(void);
#else
# define disable_simulated_io_errors()
# define enable_simulated_io_errors()
#endif

#endif /* _PAGER_H_ */

/************** End of pager.h ***********************************************/
/************** Continuing where we left off in sqliteInt.h ******************/

#ifdef SQLITE_MEMDEBUG
/*
** The following global variables are used for testing and debugging
** only.  They only work if SQLITE_MEMDEBUG is defined.
*/
SQLITE_API extern int sqlite3_nMalloc;      /* Number of sqliteMalloc() calls */
SQLITE_API extern int sqlite3_nFree;        /* Number of sqliteFree() calls */
SQLITE_API extern int sqlite3_iMallocFail;  /* Fail sqliteMalloc() after this many calls */
SQLITE_API extern int sqlite3_iMallocReset; /* Set iMallocFail to this when it reaches 0 */

SQLITE_API extern void *sqlite3_pFirst;         /* Pointer to linked list of allocations */
SQLITE_API extern int sqlite3_nMaxAlloc;        /* High water mark of ThreadData.nAlloc */
SQLITE_API extern int sqlite3_mallocDisallowed; /* assert() in sqlite3Malloc() if set */
SQLITE_API extern int sqlite3_isFail;           /* True if all malloc calls should fail */
SQLITE_API extern const char *sqlite3_zFile;    /* Filename to associate debug info with */
SQLITE_API extern int sqlite3_iLine;            /* Line number for debug info */

#define ENTER_MALLOC (sqlite3_zFile = __FILE__, sqlite3_iLine = __LINE__)
#define sqliteMalloc(x)          (ENTER_MALLOC, sqlite3Malloc(x,1))
#define sqliteMallocRaw(x)       (ENTER_MALLOC, sqlite3MallocRaw(x,1))
#define sqliteRealloc(x,y)       (ENTER_MALLOC, sqlite3Realloc(x,y))
#define sqliteStrDup(x)          (ENTER_MALLOC, sqlite3StrDup(x))
#define sqliteStrNDup(x,y)       (ENTER_MALLOC, sqlite3StrNDup(x,y))
#define sqliteReallocOrFree(x,y) (ENTER_MALLOC, sqlite3ReallocOrFree(x,y))

#else

#define ENTER_MALLOC 0
#define sqliteMalloc(x)          sqlite3Malloc(x,1)
#define sqliteMallocRaw(x)       sqlite3MallocRaw(x,1)
#define sqliteRealloc(x,y)       sqlite3Realloc(x,y)
#define sqliteStrDup(x)          sqlite3StrDup(x)
#define sqliteStrNDup(x,y)       sqlite3StrNDup(x,y)
#define sqliteReallocOrFree(x,y) sqlite3ReallocOrFree(x,y)

#endif

/* Variable sqlite3MallocHasFailed is set to true after a malloc() 
** failure occurs. 
**
** The sqlite3MallocFailed() macro returns true if a malloc has failed
** in this thread since the last call to sqlite3ApiExit(), or false 
** otherwise.
*/
SQLITE_PRIVATE int sqlite3MallocHasFailed;
#define sqlite3MallocFailed() (sqlite3MallocHasFailed && sqlite3OsInMutex(1))

#define sqliteFree(x)          sqlite3FreeX(x)
#define sqliteAllocSize(x)     sqlite3AllocSize(x)

/*
** An instance of this structure might be allocated to store information
** specific to a single thread.
*/
struct ThreadData {
  int dummy;               /* So that this structure is never empty */

#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT
  int nSoftHeapLimit;      /* Suggested max mem allocation.  No limit if <0 */
  int nAlloc;              /* Number of bytes currently allocated */
  Pager *pPager;           /* Linked list of all pagers in this thread */
#endif

#ifndef SQLITE_OMIT_SHARED_CACHE
  u8 useSharedData;        /* True if shared pagers and schemas are enabled */
  BtShared *pBtree;        /* Linked list of all currently open BTrees */
#endif
};

/*
** Name of the master database table.  The master database table
** is a special table that holds the names and attributes of all
** user tables and indices.
*/
#define MASTER_NAME       "sqlite_master"
#define TEMP_MASTER_NAME  "sqlite_temp_master"

/*
** The root-page of the master database table.
*/
#define MASTER_ROOT       1

/*
** The name of the schema table.
*/
#define SCHEMA_TABLE(x)  ((!OMIT_TEMPDB)&&(x==1)?TEMP_MASTER_NAME:MASTER_NAME)

/*
** A convenience macro that returns the number of elements in
** an array.
*/
#define ArraySize(X)    (sizeof(X)/sizeof(X[0]))

/*
** Forward references to structures
*/
typedef struct AggInfo AggInfo;
typedef struct AuthContext AuthContext;
typedef struct CollSeq CollSeq;
typedef struct Column Column;
typedef struct Db Db;
typedef struct Schema Schema;
typedef struct Expr Expr;
typedef struct ExprList ExprList;
typedef struct FKey FKey;
typedef struct FuncDef FuncDef;
typedef struct IdList IdList;
typedef struct Index Index;
typedef struct KeyClass KeyClass;
typedef struct KeyInfo KeyInfo;
typedef struct Module Module;
typedef struct NameContext NameContext;
typedef struct Parse Parse;
typedef struct Select Select;
typedef struct SrcList SrcList;
typedef struct ThreadData ThreadData;
typedef struct Table Table;
typedef struct TableLock TableLock;
typedef struct Token Token;
typedef struct TriggerStack TriggerStack;
typedef struct TriggerStep TriggerStep;
typedef struct Trigger Trigger;
typedef struct WhereInfo WhereInfo;
typedef struct WhereLevel WhereLevel;

/************** Include os.h in the middle of sqliteInt.h ********************/
/************** Begin file os.h **********************************************/
/*
** 2001 September 16
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This header file (together with is companion C source-code file
** "os.c") attempt to abstract the underlying operating system so that
** the SQLite library will work on both POSIX and windows systems.
*/
#ifndef _SQLITE_OS_H_
#define _SQLITE_OS_H_

/*
** Figure out if we are dealing with Unix, Windows, or some other
** operating system.
*/
#if defined(OS_OTHER)
# if OS_OTHER==1
#   undef OS_UNIX
#   define OS_UNIX 0
#   undef OS_WIN
#   define OS_WIN 0
#   undef OS_OS2
#   define OS_OS2 0
# else
#   undef OS_OTHER
# endif
#endif
#if !defined(OS_UNIX) && !defined(OS_OTHER)
# define OS_OTHER 0
# ifndef OS_WIN
#   if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
#     define OS_WIN 1
#     define OS_UNIX 0
#     define OS_OS2 0
#   elif defined(__EMX__) || defined(_OS2) || defined(OS2) || defined(_OS2_) || defined(__OS2__)
#     define OS_WIN 0
#     define OS_UNIX 0
#     define OS_OS2 1
#   else
#     define OS_WIN 0
#     define OS_UNIX 1
#     define OS_OS2 0
#  endif
# else
#  define OS_UNIX 0
#  define OS_OS2 0
# endif
#else
# ifndef OS_WIN
#  define OS_WIN 0
# endif
#endif


/*
** Define the maximum size of a temporary filename
*/
#if OS_WIN
# include <windows.h>
# define SQLITE_TEMPNAME_SIZE (MAX_PATH+50)
#elif OS_OS2
# if (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ >= 3) && defined(OS2_HIGH_MEMORY)
#  include <os2safe.h> /* has to be included before os2.h for linking to work */
# endif
# define INCL_DOSDATETIME
# define INCL_DOSFILEMGR
# define INCL_DOSERRORS
# define INCL_DOSMISC
# define INCL_DOSPROCESS
# define INCL_DOSMODULEMGR
# include <os2.h>
# define SQLITE_TEMPNAME_SIZE (CCHMAXPATHCOMP)
#else
# define SQLITE_TEMPNAME_SIZE 200
#endif

/* If the SET_FULLSYNC macro is not defined above, then make it
** a no-op
*/
#ifndef SET_FULLSYNC
# define SET_FULLSYNC(x,y)
#endif

/*
** The default size of a disk sector
*/
#ifndef SQLITE_DEFAULT_SECTOR_SIZE
# define SQLITE_DEFAULT_SECTOR_SIZE 512
#endif

/*
** Temporary files are named starting with this prefix followed by 16 random
** alphanumeric characters, and no file extension. They are stored in the
** OS's standard temporary file directory, and are deleted prior to exit.
** If sqlite is being embedded in another program, you may wish to change the
** prefix to reflect your program's name, so that if your program exits
** prematurely, old temporary files can be easily identified. This can be done
** using -DTEMP_FILE_PREFIX=myprefix_ on the compiler command line.
**
** 2006-10-31:  The default prefix used to be "sqlite_".  But then
** Mcafee started using SQLite in their anti-virus product and it
** started putting files with the "sqlite" name in the c:/temp folder.
** This annoyed many windows users.  Those users would then do a 
** Google search for "sqlite", find the telephone numbers of the
** developers and call to wake them up at night and complain.
** For this reason, the default name prefix is changed to be "sqlite" 
** spelled backwards.  So the temp files are still identified, but
** anybody smart enough to figure out the code is also likely smart
** enough to know that calling the developer will not help get rid
** of the file.
*/
#ifndef TEMP_FILE_PREFIX
# define TEMP_FILE_PREFIX "etilqs_"
#endif

/*
** Define the interfaces for Unix, Windows, and OS/2.
*/
#if OS_UNIX
#define sqlite3OsOpenReadWrite      sqlite3UnixOpenReadWrite
#define sqlite3OsOpenExclusive      sqlite3UnixOpenExclusive
#define sqlite3OsOpenReadOnly       sqlite3UnixOpenReadOnly
#define sqlite3OsDelete             sqlite3UnixDelete
#define sqlite3OsFileExists         sqlite3UnixFileExists
#define sqlite3OsFullPathname       sqlite3UnixFullPathname
#define sqlite3OsIsDirWritable      sqlite3UnixIsDirWritable
#define sqlite3OsSyncDirectory      sqlite3UnixSyncDirectory
#define sqlite3OsTempFileName       sqlite3UnixTempFileName
#define sqlite3OsRandomSeed         sqlite3UnixRandomSeed
#define sqlite3OsSleep              sqlite3UnixSleep
#define sqlite3OsCurrentTime        sqlite3UnixCurrentTime
#define sqlite3OsEnterMutex         sqlite3UnixEnterMutex
#define sqlite3OsLeaveMutex         sqlite3UnixLeaveMutex
#define sqlite3OsInMutex            sqlite3UnixInMutex
#define sqlite3OsThreadSpecificData sqlite3UnixThreadSpecificData
#define sqlite3OsMalloc             sqlite3GenericMalloc
#define sqlite3OsRealloc            sqlite3GenericRealloc
#define sqlite3OsFree               sqlite3GenericFree
#define sqlite3OsAllocationSize     sqlite3GenericAllocationSize
#define sqlite3OsDlopen             sqlite3UnixDlopen
#define sqlite3OsDlsym              sqlite3UnixDlsym
#define sqlite3OsDlclose            sqlite3UnixDlclose
#endif
#if OS_WIN
#define sqlite3OsOpenReadWrite      sqlite3WinOpenReadWrite
#define sqlite3OsOpenExclusive      sqlite3WinOpenExclusive
#define sqlite3OsOpenReadOnly       sqlite3WinOpenReadOnly
#define sqlite3OsDelete             sqlite3WinDelete
#define sqlite3OsFileExists         sqlite3WinFileExists
#define sqlite3OsFullPathname       sqlite3WinFullPathname
#define sqlite3OsIsDirWritable      sqlite3WinIsDirWritable
#define sqlite3OsSyncDirectory      sqlite3WinSyncDirectory
#define sqlite3OsTempFileName       sqlite3WinTempFileName
#define sqlite3OsRandomSeed         sqlite3WinRandomSeed
#define sqlite3OsSleep              sqlite3WinSleep
#define sqlite3OsCurrentTime        sqlite3WinCurrentTime
#define sqlite3OsEnterMutex         sqlite3WinEnterMutex
#define sqlite3OsLeaveMutex         sqlite3WinLeaveMutex
#define sqlite3OsInMutex            sqlite3WinInMutex
#define sqlite3OsThreadSpecificData sqlite3WinThreadSpecificData
#define sqlite3OsMalloc             sqlite3GenericMalloc
#define sqlite3OsRealloc            sqlite3GenericRealloc
#define sqlite3OsFree               sqlite3GenericFree
#define sqlite3OsAllocationSize     sqlite3GenericAllocationSize
#define sqlite3OsDlopen             sqlite3WinDlopen
#define sqlite3OsDlsym              sqlite3WinDlsym
#define sqlite3OsDlclose            sqlite3WinDlclose
#endif
#if OS_OS2
#define sqlite3OsOpenReadWrite      sqlite3Os2OpenReadWrite
#define sqlite3OsOpenExclusive      sqlite3Os2OpenExclusive
#define sqlite3OsOpenReadOnly       sqlite3Os2OpenReadOnly
#define sqlite3OsDelete             sqlite3Os2Delete
#define sqlite3OsFileExists         sqlite3Os2FileExists
#define sqlite3OsFullPathname       sqlite3Os2FullPathname
#define sqlite3OsIsDirWritable      sqlite3Os2IsDirWritable
#define sqlite3OsSyncDirectory      sqlite3Os2SyncDirectory
#define sqlite3OsTempFileName       sqlite3Os2TempFileName
#define sqlite3OsRandomSeed         sqlite3Os2RandomSeed
#define sqlite3OsSleep              sqlite3Os2Sleep
#define sqlite3OsCurrentTime        sqlite3Os2CurrentTime
#define sqlite3OsEnterMutex         sqlite3Os2EnterMutex
#define sqlite3OsLeaveMutex         sqlite3Os2LeaveMutex
#define sqlite3OsInMutex            sqlite3Os2InMutex
#define sqlite3OsThreadSpecificData sqlite3Os2ThreadSpecificData
#define sqlite3OsMalloc             sqlite3GenericMalloc
#define sqlite3OsRealloc            sqlite3GenericRealloc
#define sqlite3OsFree               sqlite3GenericFree
#define sqlite3OsAllocationSize     sqlite3GenericAllocationSize
#define sqlite3OsDlopen             sqlite3Os2Dlopen
#define sqlite3OsDlsym              sqlite3Os2Dlsym
#define sqlite3OsDlclose            sqlite3Os2Dlclose
#endif




/*
** If using an alternative OS interface, then we must have an "os_other.h"
** header file available for that interface.  Presumably the "os_other.h"
** header file contains #defines similar to those above.
*/
#if OS_OTHER
# include "os_other.h"
#endif



/*
** Forward declarations
*/
typedef struct OsFile OsFile;
typedef struct IoMethod IoMethod;

/*
** An instance of the following structure contains pointers to all
** methods on an OsFile object.
*/
struct IoMethod {
  int (*xClose)(OsFile**);
  int (*xOpenDirectory)(OsFile*, const char*);
  int (*xRead)(OsFile*, void*, int amt);
  int (*xWrite)(OsFile*, const void*, int amt);
  int (*xSeek)(OsFile*, i64 offset);
  int (*xTruncate)(OsFile*, i64 size);
  int (*xSync)(OsFile*, int);
  void (*xSetFullSync)(OsFile *id, int setting);
  int (*xFileHandle)(OsFile *id);
  int (*xFileSize)(OsFile*, i64 *pSize);
  int (*xLock)(OsFile*, int);
  int (*xUnlock)(OsFile*, int);
  int (*xLockState)(OsFile *id);
  int (*xCheckReservedLock)(OsFile *id);
  int (*xSectorSize)(OsFile *id);
};

/*
** The OsFile object describes an open disk file in an OS-dependent way.
** The version of OsFile defined here is a generic version.  Each OS
** implementation defines its own subclass of this structure that contains
** additional information needed to handle file I/O.  But the pMethod
** entry (pointing to the virtual function table) always occurs first
** so that we can always find the appropriate methods.
*/
struct OsFile {
  IoMethod const *pMethod;
};

/*
** The following values may be passed as the second argument to
** sqlite3OsLock(). The various locks exhibit the following semantics:
**
** SHARED:    Any number of processes may hold a SHARED lock simultaneously.
** RESERVED:  A single process may hold a RESERVED lock on a file at
**            any time. Other processes may hold and obtain new SHARED locks.
** PENDING:   A single process may hold a PENDING lock on a file at
**            any one time. Existing SHARED locks may persist, but no new
**            SHARED locks may be obtained by other processes.
** EXCLUSIVE: An EXCLUSIVE lock precludes all other locks.
**
** PENDING_LOCK may not be passed directly to sqlite3OsLock(). Instead, a
** process that requests an EXCLUSIVE lock may actually obtain a PENDING
** lock. This can be upgraded to an EXCLUSIVE lock by a subsequent call to
** sqlite3OsLock().
*/
#define NO_LOCK         0
#define SHARED_LOCK     1
#define RESERVED_LOCK   2
#define PENDING_LOCK    3
#define EXCLUSIVE_LOCK  4

/*
** File Locking Notes:  (Mostly about windows but also some info for Unix)
**
** We cannot use LockFileEx() or UnlockFileEx() on Win95/98/ME because
** those functions are not available.  So we use only LockFile() and
** UnlockFile().
**
** LockFile() prevents not just writing but also reading by other processes.
** A SHARED_LOCK is obtained by locking a single randomly-chosen 
** byte out of a specific range of bytes. The lock byte is obtained at 
** random so two separate readers can probably access the file at the 
** same time, unless they are unlucky and choose the same lock byte.
** An EXCLUSIVE_LOCK is obtained by locking all bytes in the range.
** There can only be one writer.  A RESERVED_LOCK is obtained by locking
** a single byte of the file that is designated as the reserved lock byte.
** A PENDING_LOCK is obtained by locking a designated byte different from
** the RESERVED_LOCK byte.
**
** On WinNT/2K/XP systems, LockFileEx() and UnlockFileEx() are available,
** which means we can use reader/writer locks.  When reader/writer locks
** are used, the lock is placed on the same range of bytes that is used
** for probabilistic locking in Win95/98/ME.  Hence, the locking scheme
** will support two or more Win95 readers or two or more WinNT readers.
** But a single Win95 reader will lock out all WinNT readers and a single
** WinNT reader will lock out all other Win95 readers.
**
** The following #defines specify the range of bytes used for locking.
** SHARED_SIZE is the number of bytes available in the pool from which
** a random byte is selected for a shared lock.  The pool of bytes for
** shared locks begins at SHARED_FIRST. 
**
** These #defines are available in sqlite_aux.h so that adaptors for
** connecting SQLite to other operating systems can use the same byte
** ranges for locking.  In particular, the same locking strategy and
** byte ranges are used for Unix.  This leaves open the possiblity of having
** clients on win95, winNT, and unix all talking to the same shared file
** and all locking correctly.  To do so would require that samba (or whatever
** tool is being used for file sharing) implements locks correctly between
** windows and unix.  I'm guessing that isn't likely to happen, but by
** using the same locking range we are at least open to the possibility.
**
** Locking in windows is manditory.  For this reason, we cannot store
** actual data in the bytes used for locking.  The pager never allocates
** the pages involved in locking therefore.  SHARED_SIZE is selected so
** that all locks will fit on a single page even at the minimum page size.
** PENDING_BYTE defines the beginning of the locks.  By default PENDING_BYTE
** is set high so that we don't have to allocate an unused page except
** for very large databases.  But one should test the page skipping logic 
** by setting PENDING_BYTE low and running the entire regression suite.
**
** Changing the value of PENDING_BYTE results in a subtly incompatible
** file format.  Depending on how it is changed, you might not notice
** the incompatibility right away, even running a full regression test.
** The default location of PENDING_BYTE is the first byte past the
** 1GB boundary.
**
*/
#ifndef SQLITE_TEST
#define PENDING_BYTE      0x40000000  /* First byte past the 1GB boundary */
#else
SQLITE_API extern unsigned int sqlite3_pending_byte;
#define PENDING_BYTE sqlite3_pending_byte
#endif

#define RESERVED_BYTE     (PENDING_BYTE+1)
#define SHARED_FIRST      (PENDING_BYTE+2)
#define SHARED_SIZE       510

/*
** Prototypes for operating system interface routines.
*/
SQLITE_PRIVATE int sqlite3OsClose(OsFile**);
SQLITE_PRIVATE int sqlite3OsOpenDirectory(OsFile*, const char*);
SQLITE_PRIVATE int sqlite3OsRead(OsFile*, void*, int amt);
SQLITE_PRIVATE int sqlite3OsWrite(OsFile*, const void*, int amt);
SQLITE_PRIVATE int sqlite3OsSeek(OsFile*, i64 offset);
SQLITE_PRIVATE int sqlite3OsTruncate(OsFile*, i64 size);
SQLITE_PRIVATE int sqlite3OsSync(OsFile*, int);
SQLITE_PRIVATE void sqlite3OsSetFullSync(OsFile *id, int setting);
SQLITE_PRIVATE int sqlite3OsFileSize(OsFile*, i64 *pSize);
SQLITE_PRIVATE int sqlite3OsLock(OsFile*, int);
SQLITE_PRIVATE int sqlite3OsUnlock(OsFile*, int);
SQLITE_PRIVATE int sqlite3OsCheckReservedLock(OsFile *id);
SQLITE_PRIVATE int sqlite3OsOpenReadWrite(const char*, OsFile**, int*);
SQLITE_PRIVATE int sqlite3OsOpenExclusive(const char*, OsFile**, int);
SQLITE_PRIVATE int sqlite3OsOpenReadOnly(const char*, OsFile**);
SQLITE_PRIVATE int sqlite3OsDelete(const char*);
SQLITE_PRIVATE int sqlite3OsFileExists(const char*);
SQLITE_PRIVATE char *sqlite3OsFullPathname(const char*);
SQLITE_PRIVATE int sqlite3OsIsDirWritable(char*);
SQLITE_PRIVATE int sqlite3OsSyncDirectory(const char*);
SQLITE_PRIVATE int sqlite3OsSectorSize(OsFile *id);
SQLITE_PRIVATE int sqlite3OsTempFileName(char*);
SQLITE_PRIVATE int sqlite3OsRandomSeed(char*);
SQLITE_PRIVATE int sqlite3OsSleep(int ms);
SQLITE_PRIVATE int sqlite3OsCurrentTime(double*);
SQLITE_PRIVATE void sqlite3OsEnterMutex(void);
SQLITE_PRIVATE void sqlite3OsLeaveMutex(void);
SQLITE_PRIVATE int sqlite3OsInMutex(int);
SQLITE_PRIVATE ThreadData *sqlite3OsThreadSpecificData(int);
SQLITE_PRIVATE void *sqlite3OsMalloc(int);
SQLITE_PRIVATE void *sqlite3OsRealloc(void *, int);
SQLITE_PRIVATE void sqlite3OsFree(void *);
SQLITE_PRIVATE int sqlite3OsAllocationSize(void *);
SQLITE_PRIVATE void *sqlite3OsDlopen(const char*);
SQLITE_PRIVATE void *sqlite3OsDlsym(void*, const char*);
SQLITE_PRIVATE int sqlite3OsDlclose(void*);

#if defined(SQLITE_TEST) || defined(SQLITE_DEBUG)
SQLITE_PRIVATE   int sqlite3OsFileHandle(OsFile *id);
SQLITE_PRIVATE   int sqlite3OsLockState(OsFile *id);
#endif

/*
** If the SQLITE_ENABLE_REDEF_IO macro is defined, then the OS-layer
** interface routines are not called directly but are invoked using
** pointers to functions.  This allows the implementation of various
** OS-layer interface routines to be modified at run-time.  There are
** obscure but legitimate reasons for wanting to do this.  But for
** most users, a direct call to the underlying interface is preferable
** so the the redefinable I/O interface is turned off by default.
*/
#ifdef SQLITE_ENABLE_REDEF_IO

/*
** When redefinable I/O is enabled, a single global instance of the
** following structure holds pointers to the routines that SQLite 
** uses to talk with the underlying operating system.  Modify this
** structure (before using any SQLite API!) to accomodate perculiar
** operating system interfaces or behaviors.
*/
struct sqlite3OsVtbl {
  int (*xOpenReadWrite)(const char*, OsFile**, int*);
  int (*xOpenExclusive)(const char*, OsFile**, int);
  int (*xOpenReadOnly)(const char*, OsFile**);

  int (*xDelete)(const char*);
  int (*xFileExists)(const char*);
  char *(*xFullPathname)(const char*);
  int (*xIsDirWritable)(char*);
  int (*xSyncDirectory)(const char*);
  int (*xTempFileName)(char*);

  int (*xRandomSeed)(char*);
  int (*xSleep)(int ms);
  int (*xCurrentTime)(double*);

  void (*xEnterMutex)(void);
  void (*xLeaveMutex)(void);
  int (*xInMutex)(int);
  ThreadData *(*xThreadSpecificData)(int);

  void *(*xMalloc)(int);
  void *(*xRealloc)(void *, int);
  void (*xFree)(void *);
  int (*xAllocationSize)(void *);

  void *(*xDlopen)(const char*);
  void *(*xDlsym)(void*, const char*);
  int (*xDlclose)(void*);
};

/* Macro used to comment out routines that do not exists when there is
** no disk I/O or extension loading
*/
#ifdef SQLITE_OMIT_DISKIO
# define IF_DISKIO(X)  0
#else
# define IF_DISKIO(X)  X
#endif
#ifdef SQLITE_OMIT_LOAD_EXTENSION
# define IF_DLOPEN(X)  0
#else
# define IF_DLOPEN(X)  X
#endif


#if defined(_SQLITE_OS_C_) || defined(SQLITE_AMALGAMATION)
  /*
  ** The os.c file implements the global virtual function table.
  ** We have to put this file here because the initializers
  ** (ex: sqlite3OsRandomSeed) are macros that are about to be
  ** redefined.
  */
  struct sqlite3OsVtbl sqlite3Os = {
    IF_DISKIO( sqlite3OsOpenReadWrite ),
    IF_DISKIO( sqlite3OsOpenExclusive ),
    IF_DISKIO( sqlite3OsOpenReadOnly ),
    IF_DISKIO( sqlite3OsDelete ),
    IF_DISKIO( sqlite3OsFileExists ),
    IF_DISKIO( sqlite3OsFullPathname ),
    IF_DISKIO( sqlite3OsIsDirWritable ),
    IF_DISKIO( sqlite3OsSyncDirectory ),
    IF_DISKIO( sqlite3OsTempFileName ),
    sqlite3OsRandomSeed,
    sqlite3OsSleep,
    sqlite3OsCurrentTime,
    sqlite3OsEnterMutex,
    sqlite3OsLeaveMutex,
    sqlite3OsInMutex,
    sqlite3OsThreadSpecificData,
    sqlite3OsMalloc,
    sqlite3OsRealloc,
    sqlite3OsFree,
    sqlite3OsAllocationSize,
    IF_DLOPEN( sqlite3OsDlopen ),
    IF_DLOPEN( sqlite3OsDlsym ),
    IF_DLOPEN( sqlite3OsDlclose ),
  };
#else
  /*
  ** Files other than os.c just reference the global virtual function table. 
  */
  extern struct sqlite3OsVtbl sqlite3Os;
#endif /* _SQLITE_OS_C_ */


/* This additional API routine is available with redefinable I/O */
SQLITE_API struct sqlite3OsVtbl *sqlite3_os_switch(void);


/*
** Redefine the OS interface to go through the virtual function table
** rather than calling routines directly.
*/
#undef sqlite3OsOpenReadWrite
#undef sqlite3OsOpenExclusive
#undef sqlite3OsOpenReadOnly
#undef sqlite3OsDelete
#undef sqlite3OsFileExists
#undef sqlite3OsFullPathname
#undef sqlite3OsIsDirWritable
#undef sqlite3OsSyncDirectory
#undef sqlite3OsTempFileName
#undef sqlite3OsRandomSeed
#undef sqlite3OsSleep
#undef sqlite3OsCurrentTime
#undef sqlite3OsEnterMutex
#undef sqlite3OsLeaveMutex
#undef sqlite3OsInMutex
#undef sqlite3OsThreadSpecificData
#undef sqlite3OsMalloc
#undef sqlite3OsRealloc
#undef sqlite3OsFree
#undef sqlite3OsAllocationSize
#define sqlite3OsOpenReadWrite      sqlite3Os.xOpenReadWrite
#define sqlite3OsOpenExclusive      sqlite3Os.xOpenExclusive
#define sqlite3OsOpenReadOnly       sqlite3Os.xOpenReadOnly
#define sqlite3OsDelete             sqlite3Os.xDelete
#define sqlite3OsFileExists         sqlite3Os.xFileExists
#define sqlite3OsFullPathname       sqlite3Os.xFullPathname
#define sqlite3OsIsDirWritable      sqlite3Os.xIsDirWritable
#define sqlite3OsSyncDirectory      sqlite3Os.xSyncDirectory
#define sqlite3OsTempFileName       sqlite3Os.xTempFileName
#define sqlite3OsRandomSeed         sqlite3Os.xRandomSeed
#define sqlite3OsSleep              sqlite3Os.xSleep
#define sqlite3OsCurrentTime        sqlite3Os.xCurrentTime
#define sqlite3OsEnterMutex         sqlite3Os.xEnterMutex
#define sqlite3OsLeaveMutex         sqlite3Os.xLeaveMutex
#define sqlite3OsInMutex            sqlite3Os.xInMutex
#define sqlite3OsThreadSpecificData sqlite3Os.xThreadSpecificData
#define sqlite3OsMalloc             sqlite3Os.xMalloc
#define sqlite3OsRealloc            sqlite3Os.xRealloc
#define sqlite3OsFree               sqlite3Os.xFree
#define sqlite3OsAllocationSize     sqlite3Os.xAllocationSize

#endif /* SQLITE_ENABLE_REDEF_IO */

#endif /* _SQLITE_OS_H_ */

/************** End of os.h **************************************************/
/************** Continuing where we left off in sqliteInt.h ******************/

/*
** Each database file to be accessed by the system is an instance
** of the following structure.  There are normally two of these structures
** in the sqlite.aDb[] array.  aDb[0] is the main database file and
** aDb[1] is the database file used to hold temporary tables.  Additional
** databases may be attached.
*/
struct Db {
  char *zName;         /* Name of this database */
  Btree *pBt;          /* The B*Tree structure for this database file */
  u8 inTrans;          /* 0: not writable.  1: Transaction.  2: Checkpoint */
  u8 safety_level;     /* How aggressive at synching data to disk */
  void *pAux;               /* Auxiliary data.  Usually NULL */
  void (*xFreeAux)(void*);  /* Routine to free pAux */
  Schema *pSchema;     /* Pointer to database schema (possibly shared) */
};

/*
** An instance of the following structure stores a database schema.
**
** If there are no virtual tables configured in this schema, the
** Schema.db variable is set to NULL. After the first virtual table
** has been added, it is set to point to the database connection 
** used to create the connection. Once a virtual table has been
** added to the Schema structure and the Schema.db variable populated, 
** only that database connection may use the Schema to prepare 
** statements.
*/
struct Schema {
  int schema_cookie;   /* Database schema version number for this file */
  Hash tblHash;        /* All tables indexed by name */
  Hash idxHash;        /* All (named) indices indexed by name */
  Hash trigHash;       /* All triggers indexed by name */
  Hash aFKey;          /* Foreign keys indexed by to-table */
  Table *pSeqTab;      /* The sqlite_sequence table used by AUTOINCREMENT */
  u8 file_format;      /* Schema format version for this file */
  u8 enc;              /* Text encoding used by this database */
  u16 flags;           /* Flags associated with this schema */
  int cache_size;      /* Number of pages to use in the cache */
#ifndef SQLITE_OMIT_VIRTUALTABLE
  sqlite3 *db;         /* "Owner" connection. See comment above */
#endif
};

/*
** These macros can be used to test, set, or clear bits in the 
** Db.flags field.
*/
#define DbHasProperty(D,I,P)     (((D)->aDb[I].pSchema->flags&(P))==(P))
#define DbHasAnyProperty(D,I,P)  (((D)->aDb[I].pSchema->flags&(P))!=0)
#define DbSetProperty(D,I,P)     (D)->aDb[I].pSchema->flags|=(P)
#define DbClearProperty(D,I,P)   (D)->aDb[I].pSchema->flags&=~(P)

/*
** Allowed values for the DB.flags field.
**
** The DB_SchemaLoaded flag is set after the database schema has been
** read into internal hash tables.
**
** DB_UnresetViews means that one or more views have column names that
** have been filled out.  If the schema changes, these column names might
** changes and so the view will need to be reset.
*/
#define DB_SchemaLoaded    0x0001  /* The schema has been loaded */
#define DB_UnresetViews    0x0002  /* Some views have defined column names */
#define DB_Empty           0x0004  /* The file is empty (length 0 bytes) */


/*
** Each database is an instance of the following structure.
**
** The sqlite.lastRowid records the last insert rowid generated by an
** insert statement.  Inserts on views do not affect its value.  Each
** trigger has its own context, so that lastRowid can be updated inside
** triggers as usual.  The previous value will be restored once the trigger
** exits.  Upon entering a before or instead of trigger, lastRowid is no
** longer (since after version 2.8.12) reset to -1.
**
** The sqlite.nChange does not count changes within triggers and keeps no
** context.  It is reset at start of sqlite3_exec.
** The sqlite.lsChange represents the number of changes made by the last
** insert, update, or delete statement.  It remains constant throughout the
** length of a statement and is then updated by OP_SetCounts.  It keeps a
** context stack just like lastRowid so that the count of changes
** within a trigger is not seen outside the trigger.  Changes to views do not
** affect the value of lsChange.
** The sqlite.csChange keeps track of the number of current changes (since
** the last statement) and is used to update sqlite_lsChange.
**
** The member variables sqlite.errCode, sqlite.zErrMsg and sqlite.zErrMsg16
** store the most recent error code and, if applicable, string. The
** internal function sqlite3Error() is used to set these variables
** consistently.
*/
struct sqlite3 {
  int nDb;                      /* Number of backends currently in use */
  Db *aDb;                      /* All backends */
  int flags;                    /* Miscellanous flags. See below */
  int errCode;                  /* Most recent error code (SQLITE_*) */
  int errMask;                  /* & result codes with this before returning */
  u8 autoCommit;                /* The auto-commit flag. */
  u8 temp_store;                /* 1: file 2: memory 0: default */
  int nTable;                   /* Number of tables in the database */
  CollSeq *pDfltColl;           /* The default collating sequence (BINARY) */
  i64 lastRowid;                /* ROWID of most recent insert (see above) */
  i64 priorNewRowid;            /* Last randomly generated ROWID */
  int magic;                    /* Magic number for detect library misuse */
  int nChange;                  /* Value returned by sqlite3_changes() */
  int nTotalChange;             /* Value returned by sqlite3_total_changes() */
  struct sqlite3InitInfo {      /* Information used during initialization */
    int iDb;                    /* When back is being initialized */
    int newTnum;                /* Rootpage of table being initialized */
    u8 busy;                    /* TRUE if currently initializing */
  } init;
  int nExtension;               /* Number of loaded extensions */
  void **aExtension;            /* Array of shared libraray handles */
  struct Vdbe *pVdbe;           /* List of active virtual machines */
  int activeVdbeCnt;            /* Number of vdbes currently executing */
  void (*xTrace)(void*,const char*);        /* Trace function */
  void *pTraceArg;                          /* Argument to the trace function */
  void (*xProfile)(void*,const char*,u64);  /* Profiling function */
  void *pProfileArg;                        /* Argument to profile function */
  void *pCommitArg;                 /* Argument to xCommitCallback() */   
  int (*xCommitCallback)(void*);    /* Invoked at every commit. */
  void *pRollbackArg;               /* Argument to xRollbackCallback() */   
  void (*xRollbackCallback)(void*); /* Invoked at every commit. */
  void *pUpdateArg;
  void (*xUpdateCallback)(void*,int, const char*,const char*,sqlite_int64);
  void(*xCollNeeded)(void*,sqlite3*,int eTextRep,const char*);
  void(*xCollNeeded16)(void*,sqlite3*,int eTextRep,const void*);
  void *pCollNeededArg;
  sqlite3_value *pErr;          /* Most recent error message */
  char *zErrMsg;                /* Most recent error message (UTF-8 encoded) */
  char *zErrMsg16;              /* Most recent error message (UTF-16 encoded) */
  union {
    int isInterrupted;          /* True if sqlite3_interrupt has been called */
    double notUsed1;            /* Spacer */
  } u1;
#ifndef SQLITE_OMIT_AUTHORIZATION
  int (*xAuth)(void*,int,const char*,const char*,const char*,const char*);
                                /* Access authorization function */
  void *pAuthArg;               /* 1st argument to the access auth function */
#endif
#ifndef SQLITE_OMIT_PROGRESS_CALLBACK
  int (*xProgress)(void *);     /* The progress callback */
  void *pProgressArg;           /* Argument to the progress callback */
  int nProgressOps;             /* Number of opcodes for progress callback */
#endif
#ifndef SQLITE_OMIT_VIRTUALTABLE
  Hash aModule;                 /* populated by sqlite3_create_module() */
  Table *pVTab;                 /* vtab with active Connect/Create method */
  sqlite3_vtab **aVTrans;       /* Virtual tables with open transactions */
  int nVTrans;                  /* Allocated size of aVTrans */
#endif
  Hash aFunc;                   /* All functions that can be in SQL exprs */
  Hash aCollSeq;                /* All collating sequences */
  BusyHandler busyHandler;      /* Busy callback */
  int busyTimeout;              /* Busy handler timeout, in msec */
  Db aDbStatic[2];              /* Static space for the 2 default backends */
#ifdef SQLITE_SSE
  sqlite3_stmt *pFetch;         /* Used by SSE to fetch stored statements */
#endif
  u8 dfltLockMode;              /* Default locking-mode for attached dbs */
};

/*
** A macro to discover the encoding of a database.
*/
#define ENC(db) ((db)->aDb[0].pSchema->enc)

/*
** Possible values for the sqlite.flags and or Db.flags fields.
**
** On sqlite.flags, the SQLITE_InTrans value means that we have
** executed a BEGIN.  On Db.flags, SQLITE_InTrans means a statement
** transaction is active on that particular database file.
*/
#define SQLITE_VdbeTrace      0x00000001  /* True to trace VDBE execution */
#define SQLITE_InTrans        0x00000008  /* True if in a transaction */
#define SQLITE_InternChanges  0x00000010  /* Uncommitted Hash table changes */
#define SQLITE_FullColNames   0x00000020  /* Show full column names on SELECT */
#define SQLITE_ShortColNames  0x00000040  /* Show short columns names */
#define SQLITE_CountRows      0x00000080  /* Count rows changed by INSERT, */
                                          /*   DELETE, or UPDATE and return */
                                          /*   the count using a callback. */
#define SQLITE_NullCallback   0x00000100  /* Invoke the callback once if the */
                                          /*   result set is empty */
#define SQLITE_SqlTrace       0x00000200  /* Debug print SQL as it executes */
#define SQLITE_VdbeListing    0x00000400  /* Debug listings of VDBE programs */
#define SQLITE_WriteSchema    0x00000800  /* OK to update SQLITE_MASTER */
#define SQLITE_NoReadlock     0x00001000  /* Readlocks are omitted when 
                                          ** accessing read-only databases */
#define SQLITE_IgnoreChecks   0x00002000  /* Do not enforce check constraints */
#define SQLITE_ReadUncommitted 0x00004000 /* For shared-cache mode */
#define SQLITE_LegacyFileFmt  0x00008000  /* Create new databases in format 1 */
#define SQLITE_FullFSync      0x00010000  /* Use full fsync on the backend */
#define SQLITE_LoadExtension  0x00020000  /* Enable load_extension */

#define SQLITE_RecoveryMode   0x00040000  /* Ignore schema errors */

/*
** Possible values for the sqlite.magic field.
** The numbers are obtained at random and have no special meaning, other
** than being distinct from one another.
*/
#define SQLITE_MAGIC_OPEN     0xa029a697  /* Database is open */
#define SQLITE_MAGIC_CLOSED   0x9f3c2d33  /* Database is closed */
#define SQLITE_MAGIC_BUSY     0xf03b7906  /* Database currently in use */
#define SQLITE_MAGIC_ERROR    0xb5357930  /* An SQLITE_MISUSE error occurred */

/*
** Each SQL function is defined by an instance of the following
** structure.  A pointer to this structure is stored in the sqlite.aFunc
** hash table.  When multiple functions have the same name, the hash table
** points to a linked list of these structures.
*/
struct FuncDef {
  i16 nArg;            /* Number of arguments.  -1 means unlimited */
  u8 iPrefEnc;         /* Preferred text encoding (SQLITE_UTF8, 16LE, 16BE) */
  u8 needCollSeq;      /* True if sqlite3GetFuncCollSeq() might be called */
  u8 flags;            /* Some combination of SQLITE_FUNC_* */
  void *pUserData;     /* User data parameter */
  FuncDef *pNext;      /* Next function with same name */
  void (*xFunc)(sqlite3_context*,int,sqlite3_value**); /* Regular function */
  void (*xStep)(sqlite3_context*,int,sqlite3_value**); /* Aggregate step */
  void (*xFinalize)(sqlite3_context*);                /* Aggregate finializer */
  char zName[1];       /* SQL name of the function.  MUST BE LAST */
};

/*
** Each SQLite module (virtual table definition) is defined by an
** instance of the following structure, stored in the sqlite3.aModule
** hash table.
*/
struct Module {
  const sqlite3_module *pModule;       /* Callback pointers */
  const char *zName;                   /* Name passed to create_module() */
  void *pAux;                          /* pAux passed to create_module() */
  void (*xDestroy)(void *);            /* Module destructor function */
};

/*
** Possible values for FuncDef.flags
*/
#define SQLITE_FUNC_LIKE   0x01  /* Candidate for the LIKE optimization */
#define SQLITE_FUNC_CASE   0x02  /* Case-sensitive LIKE-type function */
#define SQLITE_FUNC_EPHEM  0x04  /* Ephermeral.  Delete with VDBE */

/*
** information about each column of an SQL table is held in an instance
** of this structure.
*/
struct Column {
  char *zName;     /* Name of this column */
  Expr *pDflt;     /* Default value of this column */
  char *zType;     /* Data type for this column */
  char *zColl;     /* Collating sequence.  If NULL, use the default */
  u8 notNull;      /* True if there is a NOT NULL constraint */
  u8 isPrimKey;    /* True if this column is part of the PRIMARY KEY */
  char affinity;   /* One of the SQLITE_AFF_... values */
#ifndef SQLITE_OMIT_VIRTUALTABLE
  u8 isHidden;     /* True if this column is 'hidden' */
#endif
};

/*
** A "Collating Sequence" is defined by an instance of the following
** structure. Conceptually, a collating sequence consists of a name and
** a comparison routine that defines the order of that sequence.
**
** There may two seperate implementations of the collation function, one
** that processes text in UTF-8 encoding (CollSeq.xCmp) and another that
** processes text encoded in UTF-16 (CollSeq.xCmp16), using the machine
** native byte order. When a collation sequence is invoked, SQLite selects
** the version that will require the least expensive encoding
** translations, if any.
**
** The CollSeq.pUser member variable is an extra parameter that passed in
** as the first argument to the UTF-8 comparison function, xCmp.
** CollSeq.pUser16 is the equivalent for the UTF-16 comparison function,
** xCmp16.
**
** If both CollSeq.xCmp and CollSeq.xCmp16 are NULL, it means that the
** collating sequence is undefined.  Indices built on an undefined
** collating sequence may not be read or written.
*/
struct CollSeq {
  char *zName;          /* Name of the collating sequence, UTF-8 encoded */
  u8 enc;               /* Text encoding handled by xCmp() */
  u8 type;              /* One of the SQLITE_COLL_... values below */
  void *pUser;          /* First argument to xCmp() */
  int (*xCmp)(void*,int, const void*, int, const void*);
  void (*xDel)(void*);  /* Destructor for pUser */
};

/*
** Allowed values of CollSeq flags:
*/
#define SQLITE_COLL_BINARY  1  /* The default memcmp() collating sequence */
#define SQLITE_COLL_NOCASE  2  /* The built-in NOCASE collating sequence */
#define SQLITE_COLL_REVERSE 3  /* The built-in REVERSE collating sequence */
#define SQLITE_COLL_USER    0  /* Any other user-defined collating sequence */

/*
** A sort order can be either ASC or DESC.
*/
#define SQLITE_SO_ASC       0  /* Sort in ascending order */
#define SQLITE_SO_DESC      1  /* Sort in ascending order */

/*
** Column affinity types.
**
** These used to have mnemonic name like 'i' for SQLITE_AFF_INTEGER and
** 't' for SQLITE_AFF_TEXT.  But we can save a little space and improve
** the speed a little by number the values consecutively.  
**
** But rather than start with 0 or 1, we begin with 'a'.  That way,
** when multiple affinity types are concatenated into a string and
** used as the P3 operand, they will be more readable.
**
** Note also that the numeric types are grouped together so that testing
** for a numeric type is a single comparison.
*/
#define SQLITE_AFF_TEXT     'a'
#define SQLITE_AFF_NONE     'b'
#define SQLITE_AFF_NUMERIC  'c'
#define SQLITE_AFF_INTEGER  'd'
#define SQLITE_AFF_REAL     'e'

#define sqlite3IsNumericAffinity(X)  ((X)>=SQLITE_AFF_NUMERIC)

/*
** Each SQL table is represented in memory by an instance of the
** following structure.
**
** Table.zName is the name of the table.  The case of the original
** CREATE TABLE statement is stored, but case is not significant for
** comparisons.
**
** Table.nCol is the number of columns in this table.  Table.aCol is a
** pointer to an array of Column structures, one for each column.
**
** If the table has an INTEGER PRIMARY KEY, then Table.iPKey is the index of
** the column that is that key.   Otherwise Table.iPKey is negative.  Note
** that the datatype of the PRIMARY KEY must be INTEGER for this field to
** be set.  An INTEGER PRIMARY KEY is used as the rowid for each row of
** the table.  If a table has no INTEGER PRIMARY KEY, then a random rowid
** is generated for each row of the table.  Table.hasPrimKey is true if
** the table has any PRIMARY KEY, INTEGER or otherwise.
**
** Table.tnum is the page number for the root BTree page of the table in the
** database file.  If Table.iDb is the index of the database table backend
** in sqlite.aDb[].  0 is for the main database and 1 is for the file that
** holds temporary tables and indices.  If Table.isEphem
** is true, then the table is stored in a file that is automatically deleted
** when the VDBE cursor to the table is closed.  In this case Table.tnum 
** refers VDBE cursor number that holds the table open, not to the root
** page number.  Transient tables are used to hold the results of a
** sub-query that appears instead of a real table name in the FROM clause 
** of a SELECT statement.
*/
struct Table {
  char *zName;     /* Name of the table */
  int nCol;        /* Number of columns in this table */
  Column *aCol;    /* Information about each column */
  int iPKey;       /* If not less then 0, use aCol[iPKey] as the primary key */
  Index *pIndex;   /* List of SQL indexes on this table. */
  int tnum;        /* Root BTree node for this table (see note above) */
  Select *pSelect; /* NULL for tables.  Points to definition if a view. */
  int nRef;          /* Number of pointers to this Table */
  Trigger *pTrigger; /* List of SQL triggers on this table */
  FKey *pFKey;       /* Linked list of all foreign keys in this table */
  char *zColAff;     /* String defining the affinity of each column */
#ifndef SQLITE_OMIT_CHECK
  Expr *pCheck;      /* The AND of all CHECK constraints */
#endif
#ifndef SQLITE_OMIT_ALTERTABLE
  int addColOffset;  /* Offset in CREATE TABLE statement to add a new column */
#endif
  u8 readOnly;     /* True if this table should not be written by the user */
  u8 isEphem;      /* True if created using OP_OpenEphermeral */
  u8 hasPrimKey;   /* True if there exists a primary key */
  u8 keyConf;      /* What to do in case of uniqueness conflict on iPKey */
  u8 autoInc;      /* True if the integer primary key is autoincrement */
#ifndef SQLITE_OMIT_VIRTUALTABLE
  u8 isVirtual;             /* True if this is a virtual table */
  u8 isCommit;              /* True once the CREATE TABLE has been committed */
  Module *pMod;             /* Pointer to the implementation of the module */
  sqlite3_vtab *pVtab;      /* Pointer to the module instance */
  int nModuleArg;           /* Number of arguments to the module */
  char **azModuleArg;       /* Text of all module args. [0] is module name */
#endif
  Schema *pSchema;
};

/*
** Test to see whether or not a table is a virtual table.  This is
** done as a macro so that it will be optimized out when virtual
** table support is omitted from the build.
*/
#ifndef SQLITE_OMIT_VIRTUALTABLE
#  define IsVirtual(X)      ((X)->isVirtual)
#  define IsHiddenColumn(X) ((X)->isHidden)
#else
#  define IsVirtual(X)      0
#  define IsHiddenColumn(X) 0
#endif

/*
** Each foreign key constraint is an instance of the following structure.
**
** A foreign key is associated with two tables.  The "from" table is
** the table that contains the REFERENCES clause that creates the foreign
** key.  The "to" table is the table that is named in the REFERENCES clause.
** Consider this example:
**
**     CREATE TABLE ex1(
**       a INTEGER PRIMARY KEY,
**       b INTEGER CONSTRAINT fk1 REFERENCES ex2(x)
**     );
**
** For foreign key "fk1", the from-table is "ex1" and the to-table is "ex2".
**
** Each REFERENCES clause generates an instance of the following structure
** which is attached to the from-table.  The to-table need not exist when
** the from-table is created.  The existance of the to-table is not checked
** until an attempt is made to insert data into the from-table.
**
** The sqlite.aFKey hash table stores pointers to this structure
** given the name of a to-table.  For each to-table, all foreign keys
** associated with that table are on a linked list using the FKey.pNextTo
** field.
*/
struct FKey {
  Table *pFrom;     /* The table that constains the REFERENCES clause */
  FKey *pNextFrom;  /* Next foreign key in pFrom */
  char *zTo;        /* Name of table that the key points to */
  FKey *pNextTo;    /* Next foreign key that points to zTo */
  int nCol;         /* Number of columns in this key */
  struct sColMap {  /* Mapping of columns in pFrom to columns in zTo */
    int iFrom;         /* Index of column in pFrom */
    char *zCol;        /* Name of column in zTo.  If 0 use PRIMARY KEY */
  } *aCol;          /* One entry for each of nCol column s */
  u8 isDeferred;    /* True if constraint checking is deferred till COMMIT */
  u8 updateConf;    /* How to resolve conflicts that occur on UPDATE */
  u8 deleteConf;    /* How to resolve conflicts that occur on DELETE */
  u8 insertConf;    /* How to resolve conflicts that occur on INSERT */
};

/*
** SQLite supports many different ways to resolve a contraint
** error.  ROLLBACK processing means that a constraint violation
** causes the operation in process to fail and for the current transaction
** to be rolled back.  ABORT processing means the operation in process
** fails and any prior changes from that one operation are backed out,
** but the transaction is not rolled back.  FAIL processing means that
** the operation in progress stops and returns an error code.  But prior
** changes due to the same operation are not backed out and no rollback
** occurs.  IGNORE means that the particular row that caused the constraint
** error is not inserted or updated.  Processing continues and no error
** is returned.  REPLACE means that preexisting database rows that caused
** a UNIQUE constraint violation are removed so that the new insert or
** update can proceed.  Processing continues and no error is reported.
**
** RESTRICT, SETNULL, and CASCADE actions apply only to foreign keys.
** RESTRICT is the same as ABORT for IMMEDIATE foreign keys and the
** same as ROLLBACK for DEFERRED keys.  SETNULL means that the foreign
** key is set to NULL.  CASCADE means that a DELETE or UPDATE of the
** referenced table row is propagated into the row that holds the
** foreign key.
** 
** The following symbolic values are used to record which type
** of action to take.
*/
#define OE_None     0   /* There is no constraint to check */
#define OE_Rollback 1   /* Fail the operation and rollback the transaction */
#define OE_Abort    2   /* Back out changes but do no rollback transaction */
#define OE_Fail     3   /* Stop the operation but leave all prior changes */
#define OE_Ignore   4   /* Ignore the error. Do not do the INSERT or UPDATE */
#define OE_Replace  5   /* Delete existing record, then do INSERT or UPDATE */

#define OE_Restrict 6   /* OE_Abort for IMMEDIATE, OE_Rollback for DEFERRED */
#define OE_SetNull  7   /* Set the foreign key value to NULL */
#define OE_SetDflt  8   /* Set the foreign key value to its default */
#define OE_Cascade  9   /* Cascade the changes */

#define OE_Default  99  /* Do whatever the default action is */


/*
** An instance of the following structure is passed as the first
** argument to sqlite3VdbeKeyCompare and is used to control the 
** comparison of the two index keys.
**
** If the KeyInfo.incrKey value is true and the comparison would
** otherwise be equal, then return a result as if the second key
** were larger.
*/
struct KeyInfo {
  u8 enc;             /* Text encoding - one of the TEXT_Utf* values */
  u8 incrKey;         /* Increase 2nd key by epsilon before comparison */
  int nField;         /* Number of entries in aColl[] */
  u8 *aSortOrder;     /* If defined an aSortOrder[i] is true, sort DESC */
  CollSeq *aColl[1];  /* Collating sequence for each term of the key */
};

/*
** Each SQL index is represented in memory by an
** instance of the following structure.
**
** The columns of the table that are to be indexed are described
** by the aiColumn[] field of this structure.  For example, suppose
** we have the following table and index:
**
**     CREATE TABLE Ex1(c1 int, c2 int, c3 text);
**     CREATE INDEX Ex2 ON Ex1(c3,c1);
**
** In the Table structure describing Ex1, nCol==3 because there are
** three columns in the table.  In the Index structure describing
** Ex2, nColumn==2 since 2 of the 3 columns of Ex1 are indexed.
** The value of aiColumn is {2, 0}.  aiColumn[0]==2 because the 
** first column to be indexed (c3) has an index of 2 in Ex1.aCol[].
** The second column to be indexed (c1) has an index of 0 in
** Ex1.aCol[], hence Ex2.aiColumn[1]==0.
**
** The Index.onError field determines whether or not the indexed columns
** must be unique and what to do if they are not.  When Index.onError=OE_None,
** it means this is not a unique index.  Otherwise it is a unique index
** and the value of Index.onError indicate the which conflict resolution 
** algorithm to employ whenever an attempt is made to insert a non-unique
** element.
*/
struct Index {
  char *zName;     /* Name of this index */
  int nColumn;     /* Number of columns in the table used by this index */
  int *aiColumn;   /* Which columns are used by this index.  1st is 0 */
  unsigned *aiRowEst; /* Result of ANALYZE: Est. rows selected by each column */
  Table *pTable;   /* The SQL table being indexed */
  int tnum;        /* Page containing root of this index in database file */
  u8 onError;      /* OE_Abort, OE_Ignore, OE_Replace, or OE_None */
  u8 autoIndex;    /* True if is automatically created (ex: by UNIQUE) */
  char *zColAff;   /* String defining the affinity of each column */
  Index *pNext;    /* The next index associated with the same table */
  Schema *pSchema; /* Schema containing this index */
  u8 *aSortOrder;  /* Array of size Index.nColumn. True==DESC, False==ASC */
  char **azColl;   /* Array of collation sequence names for index */
};

/*
** Each token coming out of the lexer is an instance of
** this structure.  Tokens are also used as part of an expression.
**
** Note if Token.z==0 then Token.dyn and Token.n are undefined and
** may contain random values.  Do not make any assuptions about Token.dyn
** and Token.n when Token.z==0.
*/
struct Token {
  const unsigned char *z; /* Text of the token.  Not NULL-terminated! */
  unsigned dyn  : 1;      /* True for malloced memory, false for static */
  unsigned n    : 31;     /* Number of characters in this token */
};

/*
** An instance of this structure contains information needed to generate
** code for a SELECT that contains aggregate functions.
**
** If Expr.op==TK_AGG_COLUMN or TK_AGG_FUNCTION then Expr.pAggInfo is a
** pointer to this structure.  The Expr.iColumn field is the index in
** AggInfo.aCol[] or AggInfo.aFunc[] of information needed to generate
** code for that node.
**
** AggInfo.pGroupBy and AggInfo.aFunc.pExpr point to fields within the
** original Select structure that describes the SELECT statement.  These
** fields do not need to be freed when deallocating the AggInfo structure.
*/
struct AggInfo {
  u8 directMode;          /* Direct rendering mode means take data directly
                          ** from source tables rather than from accumulators */
  u8 useSortingIdx;       /* In direct mode, reference the sorting index rather
                          ** than the source table */
  int sortingIdx;         /* Cursor number of the sorting index */
  ExprList *pGroupBy;     /* The group by clause */
  int nSortingColumn;     /* Number of columns in the sorting index */
  struct AggInfo_col {    /* For each column used in source tables */
    Table *pTab;             /* Source table */
    int iTable;              /* Cursor number of the source table */
    int iColumn;             /* Column number within the source table */
    int iSorterColumn;       /* Column number in the sorting index */
    int iMem;                /* Memory location that acts as accumulator */
    Expr *pExpr;             /* The original expression */
  } *aCol;
  int nColumn;            /* Number of used entries in aCol[] */
  int nColumnAlloc;       /* Number of slots allocated for aCol[] */
  int nAccumulator;       /* Number of columns that show through to the output.
                          ** Additional columns are used only as parameters to
                          ** aggregate functions */
  struct AggInfo_func {   /* For each aggregate function */
    Expr *pExpr;             /* Expression encoding the function */
    FuncDef *pFunc;          /* The aggregate function implementation */
    int iMem;                /* Memory location that acts as accumulator */
    int iDistinct;           /* Ephermeral table used to enforce DISTINCT */
  } *aFunc;
  int nFunc;              /* Number of entries in aFunc[] */
  int nFuncAlloc;         /* Number of slots allocated for aFunc[] */
};

/*
** Each node of an expression in the parse tree is an instance
** of this structure.
**
** Expr.op is the opcode.  The integer parser token codes are reused
** as opcodes here.  For example, the parser defines TK_GE to be an integer
** code representing the ">=" operator.  This same integer code is reused
** to represent the greater-than-or-equal-to operator in the expression
** tree.
**
** Expr.pRight and Expr.pLeft are subexpressions.  Expr.pList is a list
** of argument if the expression is a function.
**
** Expr.token is the operator token for this node.  For some expressions
** that have subexpressions, Expr.token can be the complete text that gave
** rise to the Expr.  In the latter case, the token is marked as being
** a compound token.
**
** An expression of the form ID or ID.ID refers to a column in a table.
** For such expressions, Expr.op is set to TK_COLUMN and Expr.iTable is
** the integer cursor number of a VDBE cursor pointing to that table and
** Expr.iColumn is the column number for the specific column.  If the
** expression is used as a result in an aggregate SELECT, then the
** value is also stored in the Expr.iAgg column in the aggregate so that
** it can be accessed after all aggregates are computed.
**
** If the expression is a function, the Expr.iTable is an integer code
** representing which function.  If the expression is an unbound variable
** marker (a question mark character '?' in the original SQL) then the
** Expr.iTable holds the index number for that variable.
**
** If the expression is a subquery then Expr.iColumn holds an integer
** register number containing the result of the subquery.  If the
** subquery gives a constant result, then iTable is -1.  If the subquery
** gives a different answer at different times during statement processing
** then iTable is the address of a subroutine that computes the subquery.
**
** The Expr.pSelect field points to a SELECT statement.  The SELECT might
** be the right operand of an IN operator.  Or, if a scalar SELECT appears
** in an expression the opcode is TK_SELECT and Expr.pSelect is the only
** operand.
**
** If the Expr is of type OP_Column, and the table it is selecting from
** is a disk table or the "old.*" pseudo-table, then pTab points to the
** corresponding table definition.
*/
struct Expr {
  u8 op;                 /* Operation performed by this node */
  char affinity;         /* The affinity of the column or 0 if not a column */
  u16 flags;             /* Various flags.  See below */
  CollSeq *pColl;        /* The collation type of the column or 0 */
  Expr *pLeft, *pRight;  /* Left and right subnodes */
  ExprList *pList;       /* A list of expressions used as function arguments
                         ** or in "<expr> IN (<expr-list)" */
  Token token;           /* An operand token */
  Token span;            /* Complete text of the expression */
  int iTable, iColumn;   /* When op==TK_COLUMN, then this expr node means the
                         ** iColumn-th field of the iTable-th table. */
  AggInfo *pAggInfo;     /* Used by TK_AGG_COLUMN and TK_AGG_FUNCTION */
  int iAgg;              /* Which entry in pAggInfo->aCol[] or ->aFunc[] */
  int iRightJoinTable;   /* If EP_FromJoin, the right table of the join */
  Select *pSelect;       /* When the expression is a sub-select.  Also the
                         ** right side of "<expr> IN (<select>)" */
  Table *pTab;           /* Table for OP_Column expressions. */
  Schema *pSchema;
#if SQLITE_MAX_EXPR_DEPTH>0
  int nHeight;           /* Height of the tree headed by this node */
#endif
};

/*
** The following are the meanings of bits in the Expr.flags field.
*/
#define EP_FromJoin     0x01  /* Originated in ON or USING clause of a join */
#define EP_Agg          0x02  /* Contains one or more aggregate functions */
#define EP_Resolved     0x04  /* IDs have been resolved to COLUMNs */
#define EP_Error        0x08  /* Expression contains one or more errors */
#define EP_Distinct     0x10  /* Aggregate function with DISTINCT keyword */
#define EP_VarSelect    0x20  /* pSelect is correlated, not constant */
#define EP_Dequoted     0x40  /* True if the string has been dequoted */
#define EP_InfixFunc    0x80  /* True for an infix function: LIKE, GLOB, etc */
#define EP_ExpCollate  0x100  /* Collating sequence specified explicitly */

/*
** These macros can be used to test, set, or clear bits in the 
** Expr.flags field.
*/
#define ExprHasProperty(E,P)     (((E)->flags&(P))==(P))
#define ExprHasAnyProperty(E,P)  (((E)->flags&(P))!=0)
#define ExprSetProperty(E,P)     (E)->flags|=(P)
#define ExprClearProperty(E,P)   (E)->flags&=~(P)

/*
** A list of expressions.  Each expression may optionally have a
** name.  An expr/name combination can be used in several ways, such
** as the list of "expr AS ID" fields following a "SELECT" or in the
** list of "ID = expr" items in an UPDATE.  A list of expressions can
** also be used as the argument to a function, in which case the a.zName
** field is not used.
*/
struct ExprList {
  int nExpr;             /* Number of expressions on the list */
  int nAlloc;            /* Number of entries allocated below */
  int iECursor;          /* VDBE Cursor associated with this ExprList */
  struct ExprList_item {
    Expr *pExpr;           /* The list of expressions */
    char *zName;           /* Token associated with this expression */
    u8 sortOrder;          /* 1 for DESC or 0 for ASC */
    u8 isAgg;              /* True if this is an aggregate like count(*) */
    u8 done;               /* A flag to indicate when processing is finished */
  } *a;                  /* One entry for each expression */
};

/*
** An instance of this structure can hold a simple list of identifiers,
** such as the list "a,b,c" in the following statements:
**
**      INSERT INTO t(a,b,c) VALUES ...;
**      CREATE INDEX idx ON t(a,b,c);
**      CREATE TRIGGER trig BEFORE UPDATE ON t(a,b,c) ...;
**
** The IdList.a.idx field is used when the IdList represents the list of
** column names after a table name in an INSERT statement.  In the statement
**
**     INSERT INTO t(a,b,c) ...
**
** If "a" is the k-th column of table "t", then IdList.a[0].idx==k.
*/
struct IdList {
  struct IdList_item {
    char *zName;      /* Name of the identifier */
    int idx;          /* Index in some Table.aCol[] of a column named zName */
  } *a;
  int nId;         /* Number of identifiers on the list */
  int nAlloc;      /* Number of entries allocated for a[] below */
};

/*
** The bitmask datatype defined below is used for various optimizations.
**
** Changing this from a 64-bit to a 32-bit type limits the number of
** tables in a join to 32 instead of 64.  But it also reduces the size
** of the library by 738 bytes on ix86.
*/
typedef u64 Bitmask;

/*
** The following structure describes the FROM clause of a SELECT statement.
** Each table or subquery in the FROM clause is a separate element of
** the SrcList.a[] array.
**
** With the addition of multiple database support, the following structure
** can also be used to describe a particular table such as the table that
** is modified by an INSERT, DELETE, or UPDATE statement.  In standard SQL,
** such a table must be a simple name: ID.  But in SQLite, the table can
** now be identified by a database name, a dot, then the table name: ID.ID.
**
** The jointype starts out showing the join type between the current table
** and the next table on the list.  The parser builds the list this way.
** But sqlite3SrcListShiftJoinType() later shifts the jointypes so that each
** jointype expresses the join between the table and the previous table.
*/
struct SrcList {
  i16 nSrc;        /* Number of tables or subqueries in the FROM clause */
  i16 nAlloc;      /* Number of entries allocated in a[] below */
  struct SrcList_item {
    char *zDatabase;  /* Name of database holding this table */
    char *zName;      /* Name of the table */
    char *zAlias;     /* The "B" part of a "A AS B" phrase.  zName is the "A" */
    Table *pTab;      /* An SQL table corresponding to zName */
    Select *pSelect;  /* A SELECT statement used in place of a table name */
    u8 isPopulated;   /* Temporary table associated with SELECT is populated */
    u8 jointype;      /* Type of join between this able and the previous */
    int iCursor;      /* The VDBE cursor number used to access this table */
    Expr *pOn;        /* The ON clause of a join */
    IdList *pUsing;   /* The USING clause of a join */
    Bitmask colUsed;  /* Bit N (1<<N) set if column N or pTab is used */
  } a[1];             /* One entry for each identifier on the list */
};

/*
** Permitted values of the SrcList.a.jointype field
*/
#define JT_INNER     0x0001    /* Any kind of inner or cross join */
#define JT_CROSS     0x0002    /* Explicit use of the CROSS keyword */
#define JT_NATURAL   0x0004    /* True for a "natural" join */
#define JT_LEFT      0x0008    /* Left outer join */
#define JT_RIGHT     0x0010    /* Right outer join */
#define JT_OUTER     0x0020    /* The "OUTER" keyword is present */
#define JT_ERROR     0x0040    /* unknown or unsupported join type */

/*
** For each nested loop in a WHERE clause implementation, the WhereInfo
** structure contains a single instance of this structure.  This structure
** is intended to be private the the where.c module and should not be
** access or modified by other modules.
**
** The pIdxInfo and pBestIdx fields are used to help pick the best
** index on a virtual table.  The pIdxInfo pointer contains indexing
** information for the i-th table in the FROM clause before reordering.
** All the pIdxInfo pointers are freed by whereInfoFree() in where.c.
** The pBestIdx pointer is a copy of pIdxInfo for the i-th table after
** FROM clause ordering.  This is a little confusing so I will repeat
** it in different words.  WhereInfo.a[i].pIdxInfo is index information 
** for WhereInfo.pTabList.a[i].  WhereInfo.a[i].pBestInfo is the
** index information for the i-th loop of the join.  pBestInfo is always
** either NULL or a copy of some pIdxInfo.  So for cleanup it is 
** sufficient to free all of the pIdxInfo pointers.
** 
*/
struct WhereLevel {
  int iFrom;            /* Which entry in the FROM clause */
  int flags;            /* Flags associated with this level */
  int iMem;             /* First memory cell used by this level */
  int iLeftJoin;        /* Memory cell used to implement LEFT OUTER JOIN */
  Index *pIdx;          /* Index used.  NULL if no index */
  int iTabCur;          /* The VDBE cursor used to access the table */
  int iIdxCur;          /* The VDBE cursor used to acesss pIdx */
  int brk;              /* Jump here to break out of the loop */
  int nxt;              /* Jump here to start the next IN combination */
  int cont;             /* Jump here to continue with the next loop cycle */
  int top;              /* First instruction of interior of the loop */
  int op, p1, p2;       /* Opcode used to terminate the loop */
  int nEq;              /* Number of == or IN constraints on this loop */
  int nIn;              /* Number of IN operators constraining this loop */
  struct InLoop {
    int iCur;              /* The VDBE cursor used by this IN operator */
    int topAddr;           /* Top of the IN loop */
  } *aInLoop;           /* Information about each nested IN operator */
  sqlite3_index_info *pBestIdx;  /* Index information for this level */

  /* The following field is really not part of the current level.  But
  ** we need a place to cache index information for each table in the
  ** FROM clause and the WhereLevel structure is a convenient place.
  */
  sqlite3_index_info *pIdxInfo;  /* Index info for n-th source table */
};

/*
** The WHERE clause processing routine has two halves.  The
** first part does the start of the WHERE loop and the second
** half does the tail of the WHERE loop.  An instance of
** this structure is returned by the first half and passed
** into the second half to give some continuity.
*/
struct WhereInfo {
  Parse *pParse;
  SrcList *pTabList;   /* List of tables in the join */
  int iTop;            /* The very beginning of the WHERE loop */
  int iContinue;       /* Jump here to continue with next record */
  int iBreak;          /* Jump here to break out of the loop */
  int nLevel;          /* Number of nested loop */
  sqlite3_index_info **apInfo;  /* Array of pointers to index info structures */
  WhereLevel a[1];     /* Information about each nest loop in the WHERE */
};

/*
** A NameContext defines a context in which to resolve table and column
** names.  The context consists of a list of tables (the pSrcList) field and
** a list of named expression (pEList).  The named expression list may
** be NULL.  The pSrc corresponds to the FROM clause of a SELECT or
** to the table being operated on by INSERT, UPDATE, or DELETE.  The
** pEList corresponds to the result set of a SELECT and is NULL for
** other statements.
**
** NameContexts can be nested.  When resolving names, the inner-most 
** context is searched first.  If no match is found, the next outer
** context is checked.  If there is still no match, the next context
** is checked.  This process continues until either a match is found
** or all contexts are check.  When a match is found, the nRef member of
** the context containing the match is incremented. 
**
** Each subquery gets a new NameContext.  The pNext field points to the
** NameContext in the parent query.  Thus the process of scanning the
** NameContext list corresponds to searching through successively outer
** subqueries looking for a match.
*/
struct NameContext {
  Parse *pParse;       /* The parser */
  SrcList *pSrcList;   /* One or more tables used to resolve names */
  ExprList *pEList;    /* Optional list of named expressions */
  int nRef;            /* Number of names resolved by this context */
  int nErr;            /* Number of errors encountered while resolving names */
  u8 allowAgg;         /* Aggregate functions allowed here */
  u8 hasAgg;           /* True if aggregates are seen */
  u8 isCheck;          /* True if resolving names in a CHECK constraint */
  int nDepth;          /* Depth of subquery recursion. 1 for no recursion */
  AggInfo *pAggInfo;   /* Information about aggregates at this level */
  NameContext *pNext;  /* Next outer name context.  NULL for outermost */
};

/*
** An instance of the following structure contains all information
** needed to generate code for a single SELECT statement.
**
** nLimit is set to -1 if there is no LIMIT clause.  nOffset is set to 0.
** If there is a LIMIT clause, the parser sets nLimit to the value of the
** limit and nOffset to the value of the offset (or 0 if there is not
** offset).  But later on, nLimit and nOffset become the memory locations
** in the VDBE that record the limit and offset counters.
**
** addrOpenEphm[] entries contain the address of OP_OpenEphemeral opcodes.
** These addresses must be stored so that we can go back and fill in
** the P3_KEYINFO and P2 parameters later.  Neither the KeyInfo nor
** the number of columns in P2 can be computed at the same time
** as the OP_OpenEphm instruction is coded because not
** enough information about the compound query is known at that point.
** The KeyInfo for addrOpenTran[0] and [1] contains collating sequences
** for the result set.  The KeyInfo for addrOpenTran[2] contains collating
** sequences for the ORDER BY clause.
*/
struct Select {
  ExprList *pEList;      /* The fields of the result */
  u8 op;                 /* One of: TK_UNION TK_ALL TK_INTERSECT TK_EXCEPT */
  u8 isDistinct;         /* True if the DISTINCT keyword is present */
  u8 isResolved;         /* True once sqlite3SelectResolve() has run. */
  u8 isAgg;              /* True if this is an aggregate query */
  u8 usesEphm;           /* True if uses an OpenEphemeral opcode */
  u8 disallowOrderBy;    /* Do not allow an ORDER BY to be attached if TRUE */
  char affinity;         /* MakeRecord with this affinity for SRT_Set */
  SrcList *pSrc;         /* The FROM clause */
  Expr *pWhere;          /* The WHERE clause */
  ExprList *pGroupBy;    /* The GROUP BY clause */
  Expr *pHaving;         /* The HAVING clause */
  ExprList *pOrderBy;    /* The ORDER BY clause */
  Select *pPrior;        /* Prior select in a compound select statement */
  Select *pRightmost;    /* Right-most select in a compound select statement */
  Expr *pLimit;          /* LIMIT expression. NULL means not used. */
  Expr *pOffset;         /* OFFSET expression. NULL means not used. */
  int iLimit, iOffset;   /* Memory registers holding LIMIT & OFFSET counters */
  int addrOpenEphm[3];   /* OP_OpenEphem opcodes related to this select */
};

/*
** The results of a select can be distributed in several ways.
*/
#define SRT_Union        1  /* Store result as keys in an index */
#define SRT_Except       2  /* Remove result from a UNION index */
#define SRT_Discard      3  /* Do not save the results anywhere */

/* The ORDER BY clause is ignored for all of the above */
#define IgnorableOrderby(X) (X<=SRT_Discard)

#define SRT_Callback     4  /* Invoke a callback with each row of result */
#define SRT_Mem          5  /* Store result in a memory cell */
#define SRT_Set          6  /* Store non-null results as keys in an index */
#define SRT_Table        7  /* Store result as data with an automatic rowid */
#define SRT_EphemTab     8  /* Create transient tab and store like SRT_Table */
#define SRT_Subroutine   9  /* Call a subroutine to handle results */
#define SRT_Exists      10  /* Store 1 if the result is not empty */

/*
** An SQL parser context.  A copy of this structure is passed through
** the parser and down into all the parser action routine in order to
** carry around information that is global to the entire parse.
**
** The structure is divided into two parts.  When the parser and code
** generate call themselves recursively, the first part of the structure
** is constant but the second part is reset at the beginning and end of
** each recursion.
**
** The nTableLock and aTableLock variables are only used if the shared-cache 
** feature is enabled (if sqlite3Tsd()->useSharedData is true). They are
** used to store the set of table-locks required by the statement being
** compiled. Function sqlite3TableLock() is used to add entries to the
** list.
*/
struct Parse {
  sqlite3 *db;         /* The main database structure */
  int rc;              /* Return code from execution */
  char *zErrMsg;       /* An error message */
  Vdbe *pVdbe;         /* An engine for executing database bytecode */
  u8 colNamesSet;      /* TRUE after OP_ColumnName has been issued to pVdbe */
  u8 nameClash;        /* A permanent table name clashes with temp table name */
  u8 checkSchema;      /* Causes schema cookie check after an error */
  u8 nested;           /* Number of nested calls to the parser/code generator */
  u8 parseError;       /* True after a parsing error.  Ticket #1794 */
  int nErr;            /* Number of errors seen */
  int nTab;            /* Number of previously allocated VDBE cursors */
  int nMem;            /* Number of memory cells used so far */
  int nSet;            /* Number of sets used so far */
  int ckOffset;        /* Stack offset to data used by CHECK constraints */
  u32 writeMask;       /* Start a write transaction on these databases */
  u32 cookieMask;      /* Bitmask of schema verified databases */
  int cookieGoto;      /* Address of OP_Goto to cookie verifier subroutine */
  int cookieValue[SQLITE_MAX_ATTACHED+2];  /* Values of cookies to verify */
#ifndef SQLITE_OMIT_SHARED_CACHE
  int nTableLock;        /* Number of locks in aTableLock */
  TableLock *aTableLock; /* Required table locks for shared-cache mode */
#endif

  /* Above is constant between recursions.  Below is reset before and after
  ** each recursion */

  int nVar;            /* Number of '?' variables seen in the SQL so far */
  int nVarExpr;        /* Number of used slots in apVarExpr[] */
  int nVarExprAlloc;   /* Number of allocated slots in apVarExpr[] */
  Expr **apVarExpr;    /* Pointers to :aaa and $aaaa wildcard expressions */
  u8 explain;          /* True if the EXPLAIN flag is found on the query */
  Token sErrToken;     /* The token at which the error occurred */
  Token sNameToken;    /* Token with unqualified schema object name */
  Token sLastToken;    /* The last token parsed */
  const char *zSql;    /* All SQL text */
  const char *zTail;   /* All SQL text past the last semicolon parsed */
  Table *pNewTable;    /* A table being constructed by CREATE TABLE */
  Trigger *pNewTrigger;     /* Trigger under construct by a CREATE TRIGGER */
  TriggerStack *trigStack;  /* Trigger actions being coded */
  const char *zAuthContext; /* The 6th parameter to db->xAuth callbacks */
#ifndef SQLITE_OMIT_VIRTUALTABLE
  Token sArg;                /* Complete text of a module argument */
  u8 declareVtab;            /* True if inside sqlite3_declare_vtab() */
  Table *pVirtualLock;       /* Require virtual table lock on this table */
#endif
#if SQLITE_MAX_EXPR_DEPTH>0
  int nHeight;            /* Expression tree height of current sub-select */
#endif
};

#ifdef SQLITE_OMIT_VIRTUALTABLE
  #define IN_DECLARE_VTAB 0
#else
  #define IN_DECLARE_VTAB (pParse->declareVtab)
#endif

/*
** An instance of the following structure can be declared on a stack and used
** to save the Parse.zAuthContext value so that it can be restored later.
*/
struct AuthContext {
  const char *zAuthContext;   /* Put saved Parse.zAuthContext here */
  Parse *pParse;              /* The Parse structure */
};

/*
** Bitfield flags for P2 value in OP_Insert and OP_Delete
*/
#define OPFLAG_NCHANGE   1    /* Set to update db->nChange */
#define OPFLAG_LASTROWID 2    /* Set to update db->lastRowid */
#define OPFLAG_ISUPDATE  4    /* This OP_Insert is an sql UPDATE */
#define OPFLAG_APPEND    8    /* This is likely to be an append */

/*
 * Each trigger present in the database schema is stored as an instance of
 * struct Trigger. 
 *
 * Pointers to instances of struct Trigger are stored in two ways.
 * 1. In the "trigHash" hash table (part of the sqlite3* that represents the 
 *    database). This allows Trigger structures to be retrieved by name.
 * 2. All triggers associated with a single table form a linked list, using the
 *    pNext member of struct Trigger. A pointer to the first element of the
 *    linked list is stored as the "pTrigger" member of the associated
 *    struct Table.
 *
 * The "step_list" member points to the first element of a linked list
 * containing the SQL statements specified as the trigger program.
 */
struct Trigger {
  char *name;             /* The name of the trigger                        */
  char *table;            /* The table or view to which the trigger applies */
  u8 op;                  /* One of TK_DELETE, TK_UPDATE, TK_INSERT         */
  u8 tr_tm;               /* One of TRIGGER_BEFORE, TRIGGER_AFTER */
  Expr *pWhen;            /* The WHEN clause of the expresion (may be NULL) */
  IdList *pColumns;       /* If this is an UPDATE OF <column-list> trigger,
                             the <column-list> is stored here */
  Token nameToken;        /* Token containing zName. Use during parsing only */
  Schema *pSchema;        /* Schema containing the trigger */
  Schema *pTabSchema;     /* Schema containing the table */
  TriggerStep *step_list; /* Link list of trigger program steps             */
  Trigger *pNext;         /* Next trigger associated with the table */
};

/*
** A trigger is either a BEFORE or an AFTER trigger.  The following constants
** determine which. 
**
** If there are multiple triggers, you might of some BEFORE and some AFTER.
** In that cases, the constants below can be ORed together.
*/
#define TRIGGER_BEFORE  1
#define TRIGGER_AFTER   2

/*
 * An instance of struct TriggerStep is used to store a single SQL statement
 * that is a part of a trigger-program. 
 *
 * Instances of struct TriggerStep are stored in a singly linked list (linked
 * using the "pNext" member) referenced by the "step_list" member of the 
 * associated struct Trigger instance. The first element of the linked list is
 * the first step of the trigger-program.
 * 
 * The "op" member indicates whether this is a "DELETE", "INSERT", "UPDATE" or
 * "SELECT" statement. The meanings of the other members is determined by the 
 * value of "op" as follows:
 *
 * (op == TK_INSERT)
 * orconf    -> stores the ON CONFLICT algorithm
 * pSelect   -> If this is an INSERT INTO ... SELECT ... statement, then
 *              this stores a pointer to the SELECT statement. Otherwise NULL.
 * target    -> A token holding the name of the table to insert into.
 * pExprList -> If this is an INSERT INTO ... VALUES ... statement, then
 *              this stores values to be inserted. Otherwise NULL.
 * pIdList   -> If this is an INSERT INTO ... (<column-names>) VALUES ... 
 *              statement, then this stores the column-names to be
 *              inserted into.
 *
 * (op == TK_DELETE)
 * target    -> A token holding the name of the table to delete from.
 * pWhere    -> The WHERE clause of the DELETE statement if one is specified.
 *              Otherwise NULL.
 * 
 * (op == TK_UPDATE)
 * target    -> A token holding the name of the table to update rows of.
 * pWhere    -> The WHERE clause of the UPDATE statement if one is specified.
 *              Otherwise NULL.
 * pExprList -> A list of the columns to update and the expressions to update
 *              them to. See sqlite3Update() documentation of "pChanges"
 *              argument.
 * 
 */
struct TriggerStep {
  int op;              /* One of TK_DELETE, TK_UPDATE, TK_INSERT, TK_SELECT */
  int orconf;          /* OE_Rollback etc. */
  Trigger *pTrig;      /* The trigger that this step is a part of */

  Select *pSelect;     /* Valid for SELECT and sometimes 
                          INSERT steps (when pExprList == 0) */
  Token target;        /* Valid for DELETE, UPDATE, INSERT steps */
  Expr *pWhere;        /* Valid for DELETE, UPDATE steps */
  ExprList *pExprList; /* Valid for UPDATE statements and sometimes 
                           INSERT steps (when pSelect == 0)         */
  IdList *pIdList;     /* Valid for INSERT statements only */
  TriggerStep *pNext;  /* Next in the link-list */
  TriggerStep *pLast;  /* Last element in link-list. Valid for 1st elem only */
};

/*
 * An instance of struct TriggerStack stores information required during code
 * generation of a single trigger program. While the trigger program is being
 * coded, its associated TriggerStack instance is pointed to by the
 * "pTriggerStack" member of the Parse structure.
 *
 * The pTab member points to the table that triggers are being coded on. The 
 * newIdx member contains the index of the vdbe cursor that points at the temp
 * table that stores the new.* references. If new.* references are not valid
 * for the trigger being coded (for example an ON DELETE trigger), then newIdx
 * is set to -1. The oldIdx member is analogous to newIdx, for old.* references.
 *
 * The ON CONFLICT policy to be used for the trigger program steps is stored 
 * as the orconf member. If this is OE_Default, then the ON CONFLICT clause 
 * specified for individual triggers steps is used.
 *
 * struct TriggerStack has a "pNext" member, to allow linked lists to be
 * constructed. When coding nested triggers (triggers fired by other triggers)
 * each nested trigger stores its parent trigger's TriggerStack as the "pNext" 
 * pointer. Once the nested trigger has been coded, the pNext value is restored
 * to the pTriggerStack member of the Parse stucture and coding of the parent
 * trigger continues.
 *
 * Before a nested trigger is coded, the linked list pointed to by the 
 * pTriggerStack is scanned to ensure that the trigger is not about to be coded
 * recursively. If this condition is detected, the nested trigger is not coded.
 */
struct TriggerStack {
  Table *pTab;         /* Table that triggers are currently being coded on */
  int newIdx;          /* Index of vdbe cursor to "new" temp table */
  int oldIdx;          /* Index of vdbe cursor to "old" temp table */
  int orconf;          /* Current orconf policy */
  int ignoreJump;      /* where to jump to for a RAISE(IGNORE) */
  Trigger *pTrigger;   /* The trigger currently being coded */
  TriggerStack *pNext; /* Next trigger down on the trigger stack */
};

/*
** The following structure contains information used by the sqliteFix...
** routines as they walk the parse tree to make database references
** explicit.  
*/
typedef struct DbFixer DbFixer;
struct DbFixer {
  Parse *pParse;      /* The parsing context.  Error messages written here */
  const char *zDb;    /* Make sure all objects are contained in this database */
  const char *zType;  /* Type of the container - used for error messages */
  const Token *pName; /* Name of the container - used for error messages */
};

/*
** A pointer to this structure is used to communicate information
** from sqlite3Init and OP_ParseSchema into the sqlite3InitCallback.
*/
typedef struct {
  sqlite3 *db;        /* The database being initialized */
  int iDb;            /* 0 for main database.  1 for TEMP, 2.. for ATTACHed */
  char **pzErrMsg;    /* Error message stored here */
  int rc;             /* Result code stored here */
} InitData;

/*
** Assuming zIn points to the first byte of a UTF-8 character,
** advance zIn to point to the first byte of the next UTF-8 character.
*/
#define SQLITE_SKIP_UTF8(zIn) {                        \
  if( (*(zIn++))>=0xc0 ){                              \
    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \
  }                                                    \
}

/*
** The SQLITE_CORRUPT_BKPT macro can be either a constant (for production
** builds) or a function call (for debugging).  If it is a function call,
** it allows the operator to set a breakpoint at the spot where database
** corruption is first detected.
*/
#ifdef SQLITE_DEBUG
SQLITE_PRIVATE   int sqlite3Corrupt(void);
# define SQLITE_CORRUPT_BKPT sqlite3Corrupt()
#else
# define SQLITE_CORRUPT_BKPT SQLITE_CORRUPT
#endif

/*
** Internal function prototypes
*/
SQLITE_PRIVATE int sqlite3StrICmp(const char *, const char *);
SQLITE_PRIVATE int sqlite3StrNICmp(const char *, const char *, int);
SQLITE_PRIVATE int sqlite3IsNumber(const char*, int*, u8);

SQLITE_PRIVATE void *sqlite3Malloc(int,int);
SQLITE_PRIVATE void *sqlite3MallocRaw(int,int);
SQLITE_PRIVATE void *sqlite3Realloc(void*,int);
SQLITE_PRIVATE char *sqlite3StrDup(const char*);
SQLITE_PRIVATE char *sqlite3StrNDup(const char*, int);
# define sqlite3CheckMemory(a,b)
SQLITE_PRIVATE void *sqlite3ReallocOrFree(void*,int);
SQLITE_PRIVATE void sqlite3FreeX(void*);
SQLITE_PRIVATE void *sqlite3MallocX(int);
#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT
SQLITE_PRIVATE   int sqlite3AllocSize(void *);
#endif

SQLITE_PRIVATE char *sqlite3MPrintf(const char*, ...);
SQLITE_PRIVATE char *sqlite3VMPrintf(const char*, va_list);
#if defined(SQLITE_TEST) || defined(SQLITE_DEBUG)
SQLITE_PRIVATE   void sqlite3DebugPrintf(const char*, ...);
SQLITE_PRIVATE   void *sqlite3TextToPtr(const char*);
#endif
SQLITE_PRIVATE void sqlite3SetString(char **, ...);
SQLITE_PRIVATE void sqlite3ErrorMsg(Parse*, const char*, ...);
SQLITE_PRIVATE void sqlite3ErrorClear(Parse*);
SQLITE_PRIVATE void sqlite3Dequote(char*);
SQLITE_PRIVATE void sqlite3DequoteExpr(Expr*);
SQLITE_PRIVATE int sqlite3KeywordCode(const unsigned char*, int);
SQLITE_PRIVATE int sqlite3RunParser(Parse*, const char*, char **);
SQLITE_PRIVATE void sqlite3FinishCoding(Parse*);
SQLITE_PRIVATE Expr *sqlite3Expr(int, Expr*, Expr*, const Token*);
SQLITE_PRIVATE Expr *sqlite3ExprOrFree(int, Expr*, Expr*, const Token*);
SQLITE_PRIVATE Expr *sqlite3RegisterExpr(Parse*,Token*);
SQLITE_PRIVATE Expr *sqlite3ExprAnd(Expr*, Expr*);
SQLITE_PRIVATE void sqlite3ExprSpan(Expr*,Token*,Token*);
SQLITE_PRIVATE Expr *sqlite3ExprFunction(ExprList*, Token*);
SQLITE_PRIVATE void sqlite3ExprAssignVarNumber(Parse*, Expr*);
SQLITE_PRIVATE void sqlite3ExprDelete(Expr*);
SQLITE_PRIVATE ExprList *sqlite3ExprListAppend(ExprList*,Expr*,Token*);
SQLITE_PRIVATE void sqlite3ExprListDelete(ExprList*);
SQLITE_PRIVATE int sqlite3Init(sqlite3*, char**);
SQLITE_PRIVATE int sqlite3InitCallback(void*, int, char**, char**);
SQLITE_PRIVATE void sqlite3Pragma(Parse*,Token*,Token*,Token*,int);
SQLITE_PRIVATE void sqlite3ResetInternalSchema(sqlite3*, int);
SQLITE_PRIVATE void sqlite3BeginParse(Parse*,int);
SQLITE_PRIVATE void sqlite3CommitInternalChanges(sqlite3*);
SQLITE_PRIVATE Table *sqlite3ResultSetOfSelect(Parse*,char*,Select*);
SQLITE_PRIVATE void sqlite3OpenMasterTable(Parse *, int);
SQLITE_PRIVATE void sqlite3StartTable(Parse*,Token*,Token*,int,int,int,int);
SQLITE_PRIVATE void sqlite3AddColumn(Parse*,Token*);
SQLITE_PRIVATE void sqlite3AddNotNull(Parse*, int);
SQLITE_PRIVATE void sqlite3AddPrimaryKey(Parse*, ExprList*, int, int, int);
SQLITE_PRIVATE void sqlite3AddCheckConstraint(Parse*, Expr*);
SQLITE_PRIVATE void sqlite3AddColumnType(Parse*,Token*);
SQLITE_PRIVATE void sqlite3AddDefaultValue(Parse*,Expr*);
SQLITE_PRIVATE void sqlite3AddCollateType(Parse*, const char*, int);
SQLITE_PRIVATE void sqlite3EndTable(Parse*,Token*,Token*,Select*);

SQLITE_PRIVATE void sqlite3CreateView(Parse*,Token*,Token*,Token*,Select*,int,int);

#if !defined(SQLITE_OMIT_VIEW) || !defined(SQLITE_OMIT_VIRTUALTABLE)
SQLITE_PRIVATE   int sqlite3ViewGetColumnNames(Parse*,Table*);
#else
# define sqlite3ViewGetColumnNames(A,B) 0
#endif

SQLITE_PRIVATE void sqlite3DropTable(Parse*, SrcList*, int, int);
SQLITE_PRIVATE void sqlite3DeleteTable(Table*);
SQLITE_PRIVATE void sqlite3Insert(Parse*, SrcList*, ExprList*, Select*, IdList*, int);
SQLITE_PRIVATE void *sqlite3ArrayAllocate(void*,int,int,int*,int*,int*);
SQLITE_PRIVATE IdList *sqlite3IdListAppend(IdList*, Token*);
SQLITE_PRIVATE int sqlite3IdListIndex(IdList*,const char*);
SQLITE_PRIVATE SrcList *sqlite3SrcListAppend(SrcList*, Token*, Token*);
SQLITE_PRIVATE SrcList *sqlite3SrcListAppendFromTerm(SrcList*, Token*, Token*, Token*,
                                      Select*, Expr*, IdList*);
SQLITE_PRIVATE void sqlite3SrcListShiftJoinType(SrcList*);
SQLITE_PRIVATE void sqlite3SrcListAssignCursors(Parse*, SrcList*);
SQLITE_PRIVATE void sqlite3IdListDelete(IdList*);
SQLITE_PRIVATE void sqlite3SrcListDelete(SrcList*);
SQLITE_PRIVATE void sqlite3CreateIndex(Parse*,Token*,Token*,SrcList*,ExprList*,int,Token*,
                        Token*, int, int);
SQLITE_PRIVATE void sqlite3DropIndex(Parse*, SrcList*, int);
SQLITE_PRIVATE int sqlite3Select(Parse*, Select*, int, int, Select*, int, int*, char *aff);
SQLITE_PRIVATE Select *sqlite3SelectNew(ExprList*,SrcList*,Expr*,ExprList*,Expr*,ExprList*,
                        int,Expr*,Expr*);
SQLITE_PRIVATE void sqlite3SelectDelete(Select*);
SQLITE_PRIVATE Table *sqlite3SrcListLookup(Parse*, SrcList*);
SQLITE_PRIVATE int sqlite3IsReadOnly(Parse*, Table*, int);
SQLITE_PRIVATE void sqlite3OpenTable(Parse*, int iCur, int iDb, Table*, int);
SQLITE_PRIVATE void sqlite3DeleteFrom(Parse*, SrcList*, Expr*);
SQLITE_PRIVATE void sqlite3Update(Parse*, SrcList*, ExprList*, Expr*, int);
SQLITE_PRIVATE WhereInfo *sqlite3WhereBegin(Parse*, SrcList*, Expr*, ExprList**);
SQLITE_PRIVATE void sqlite3WhereEnd(WhereInfo*);
SQLITE_PRIVATE void sqlite3ExprCodeGetColumn(Vdbe*, Table*, int, int);
SQLITE_PRIVATE void sqlite3ExprCode(Parse*, Expr*);
SQLITE_PRIVATE void sqlite3ExprCodeAndCache(Parse*, Expr*);
SQLITE_PRIVATE int sqlite3ExprCodeExprList(Parse*, ExprList*);
SQLITE_PRIVATE void sqlite3ExprIfTrue(Parse*, Expr*, int, int);
SQLITE_PRIVATE void sqlite3ExprIfFalse(Parse*, Expr*, int, int);
SQLITE_PRIVATE Table *sqlite3FindTable(sqlite3*,const char*, const char*);
SQLITE_PRIVATE Table *sqlite3LocateTable(Parse*,const char*, const char*);
SQLITE_PRIVATE Index *sqlite3FindIndex(sqlite3*,const char*, const char*);
SQLITE_PRIVATE void sqlite3UnlinkAndDeleteTable(sqlite3*,int,const char*);
SQLITE_PRIVATE void sqlite3UnlinkAndDeleteIndex(sqlite3*,int,const char*);
SQLITE_PRIVATE void sqlite3Vacuum(Parse*);
SQLITE_PRIVATE int sqlite3RunVacuum(char**, sqlite3*);
SQLITE_PRIVATE char *sqlite3NameFromToken(Token*);
SQLITE_PRIVATE int sqlite3ExprCompare(Expr*, Expr*);
int sqliteFuncId(Token*);
SQLITE_PRIVATE int sqlite3ExprResolveNames(NameContext *, Expr *);
SQLITE_PRIVATE int sqlite3ExprAnalyzeAggregates(NameContext*, Expr*);
SQLITE_PRIVATE int sqlite3ExprAnalyzeAggList(NameContext*,ExprList*);
SQLITE_PRIVATE Vdbe *sqlite3GetVdbe(Parse*);
SQLITE_PRIVATE Expr *sqlite3CreateIdExpr(const char*);
SQLITE_PRIVATE void sqlite3Randomness(int, void*);
SQLITE_PRIVATE void sqlite3RollbackAll(sqlite3*);
SQLITE_PRIVATE void sqlite3CodeVerifySchema(Parse*, int);
SQLITE_PRIVATE void sqlite3BeginTransaction(Parse*, int);
SQLITE_PRIVATE void sqlite3CommitTransaction(Parse*);
SQLITE_PRIVATE void sqlite3RollbackTransaction(Parse*);
SQLITE_PRIVATE int sqlite3ExprIsConstant(Expr*);
SQLITE_PRIVATE int sqlite3ExprIsConstantNotJoin(Expr*);
SQLITE_PRIVATE int sqlite3ExprIsConstantOrFunction(Expr*);
SQLITE_PRIVATE int sqlite3ExprIsInteger(Expr*, int*);
SQLITE_PRIVATE int sqlite3IsRowid(const char*);
SQLITE_PRIVATE void sqlite3GenerateRowDelete(sqlite3*, Vdbe*, Table*, int, int);
SQLITE_PRIVATE void sqlite3GenerateRowIndexDelete(Vdbe*, Table*, int, char*);
SQLITE_PRIVATE void sqlite3GenerateIndexKey(Vdbe*, Index*, int);
SQLITE_PRIVATE void sqlite3GenerateConstraintChecks(Parse*,Table*,int,char*,int,int,int,int);
SQLITE_PRIVATE void sqlite3CompleteInsertion(Parse*, Table*, int, char*, int, int, int, int);
SQLITE_PRIVATE void sqlite3OpenTableAndIndices(Parse*, Table*, int, int);
SQLITE_PRIVATE void sqlite3BeginWriteOperation(Parse*, int, int);
SQLITE_PRIVATE Expr *sqlite3ExprDup(Expr*);
SQLITE_PRIVATE void sqlite3TokenCopy(Token*, Token*);
SQLITE_PRIVATE ExprList *sqlite3ExprListDup(ExprList*);
SQLITE_PRIVATE SrcList *sqlite3SrcListDup(SrcList*);
SQLITE_PRIVATE IdList *sqlite3IdListDup(IdList*);
SQLITE_PRIVATE Select *sqlite3SelectDup(Select*);
SQLITE_PRIVATE FuncDef *sqlite3FindFunction(sqlite3*,const char*,int,int,u8,int);
SQLITE_PRIVATE void sqlite3RegisterBuiltinFunctions(sqlite3*);
SQLITE_PRIVATE void sqlite3RegisterDateTimeFunctions(sqlite3*);
SQLITE_PRIVATE int sqlite3SafetyOn(sqlite3*);
SQLITE_PRIVATE int sqlite3SafetyOff(sqlite3*);
SQLITE_PRIVATE int sqlite3SafetyCheck(sqlite3*);
SQLITE_PRIVATE void sqlite3ChangeCookie(sqlite3*, Vdbe*, int);

#ifndef SQLITE_OMIT_TRIGGER
SQLITE_PRIVATE   void sqlite3BeginTrigger(Parse*, Token*,Token*,int,int,IdList*,SrcList*,
                           Expr*,int, int);
SQLITE_PRIVATE   void sqlite3FinishTrigger(Parse*, TriggerStep*, Token*);
SQLITE_PRIVATE   void sqlite3DropTrigger(Parse*, SrcList*, int);
SQLITE_PRIVATE   void sqlite3DropTriggerPtr(Parse*, Trigger*);
SQLITE_PRIVATE   int sqlite3TriggersExist(Parse*, Table*, int, ExprList*);
SQLITE_PRIVATE   int sqlite3CodeRowTrigger(Parse*, int, ExprList*, int, Table *, int, int, 
                           int, int);
  void sqliteViewTriggers(Parse*, Table*, Expr*, int, ExprList*);
SQLITE_PRIVATE   void sqlite3DeleteTriggerStep(TriggerStep*);
SQLITE_PRIVATE   TriggerStep *sqlite3TriggerSelectStep(Select*);
SQLITE_PRIVATE   TriggerStep *sqlite3TriggerInsertStep(Token*, IdList*, ExprList*,Select*,int);
SQLITE_PRIVATE   TriggerStep *sqlite3TriggerUpdateStep(Token*, ExprList*, Expr*, int);
SQLITE_PRIVATE   TriggerStep *sqlite3TriggerDeleteStep(Token*, Expr*);
SQLITE_PRIVATE   void sqlite3DeleteTrigger(Trigger*);
SQLITE_PRIVATE   void sqlite3UnlinkAndDeleteTrigger(sqlite3*,int,const char*);
#else
# define sqlite3TriggersExist(A,B,C,D,E,F) 0
# define sqlite3DeleteTrigger(A)
# define sqlite3DropTriggerPtr(A,B)
# define sqlite3UnlinkAndDeleteTrigger(A,B,C)
# define sqlite3CodeRowTrigger(A,B,C,D,E,F,G,H,I) 0
#endif

SQLITE_PRIVATE int sqlite3JoinType(Parse*, Token*, Token*, Token*);
SQLITE_PRIVATE void sqlite3CreateForeignKey(Parse*, ExprList*, Token*, ExprList*, int);
SQLITE_PRIVATE void sqlite3DeferForeignKey(Parse*, int);
#ifndef SQLITE_OMIT_AUTHORIZATION
SQLITE_PRIVATE   void sqlite3AuthRead(Parse*,Expr*,SrcList*);
SQLITE_PRIVATE   int sqlite3AuthCheck(Parse*,int, const char*, const char*, const char*);
SQLITE_PRIVATE   void sqlite3AuthContextPush(Parse*, AuthContext*, const char*);
SQLITE_PRIVATE   void sqlite3AuthContextPop(AuthContext*);
#else
# define sqlite3AuthRead(a,b,c)
# define sqlite3AuthCheck(a,b,c,d,e)    SQLITE_OK
# define sqlite3AuthContextPush(a,b,c)
# define sqlite3AuthContextPop(a)  ((void)(a))
#endif
SQLITE_PRIVATE void sqlite3Attach(Parse*, Expr*, Expr*, Expr*);
SQLITE_PRIVATE void sqlite3Detach(Parse*, Expr*);
SQLITE_PRIVATE int sqlite3BtreeFactory(const sqlite3 *db, const char *zFilename,
                       int omitJournal, int nCache, Btree **ppBtree);
SQLITE_PRIVATE int sqlite3FixInit(DbFixer*, Parse*, int, const char*, const Token*);
SQLITE_PRIVATE int sqlite3FixSrcList(DbFixer*, SrcList*);
SQLITE_PRIVATE int sqlite3FixSelect(DbFixer*, Select*);
SQLITE_PRIVATE int sqlite3FixExpr(DbFixer*, Expr*);
SQLITE_PRIVATE int sqlite3FixExprList(DbFixer*, ExprList*);
SQLITE_PRIVATE int sqlite3FixTriggerStep(DbFixer*, TriggerStep*);
SQLITE_PRIVATE int sqlite3AtoF(const char *z, double*);
SQLITE_API char *sqlite3_snprintf(int,char*,const char*,...);
SQLITE_PRIVATE int sqlite3GetInt32(const char *, int*);
SQLITE_PRIVATE int sqlite3FitsIn64Bits(const char *);
SQLITE_PRIVATE int sqlite3Utf16ByteLen(const void *pData, int nChar);
SQLITE_PRIVATE int sqlite3Utf8CharLen(const char *pData, int nByte);
SQLITE_PRIVATE int sqlite3Utf8Read(const u8*, const u8*, const u8**);
SQLITE_PRIVATE int sqlite3PutVarint(unsigned char *, u64);
SQLITE_PRIVATE int sqlite3GetVarint(const unsigned char *, u64 *);
SQLITE_PRIVATE int sqlite3GetVarint32(const unsigned char *, u32 *);
SQLITE_PRIVATE int sqlite3VarintLen(u64 v);
SQLITE_PRIVATE void sqlite3IndexAffinityStr(Vdbe *, Index *);
SQLITE_PRIVATE void sqlite3TableAffinityStr(Vdbe *, Table *);
SQLITE_PRIVATE char sqlite3CompareAffinity(Expr *pExpr, char aff2);
SQLITE_PRIVATE int sqlite3IndexAffinityOk(Expr *pExpr, char idx_affinity);
SQLITE_PRIVATE char sqlite3ExprAffinity(Expr *pExpr);
SQLITE_PRIVATE int sqlite3Atoi64(const char*, i64*);
SQLITE_PRIVATE void sqlite3Error(sqlite3*, int, const char*,...);
SQLITE_PRIVATE void *sqlite3HexToBlob(const char *z);
SQLITE_PRIVATE int sqlite3TwoPartName(Parse *, Token *, Token *, Token **);
SQLITE_PRIVATE const char *sqlite3ErrStr(int);
SQLITE_PRIVATE int sqlite3ReadSchema(Parse *pParse);
SQLITE_PRIVATE CollSeq *sqlite3FindCollSeq(sqlite3*,u8 enc, const char *,int,int);
SQLITE_PRIVATE CollSeq *sqlite3LocateCollSeq(Parse *pParse, const char *zName, int nName);
SQLITE_PRIVATE CollSeq *sqlite3ExprCollSeq(Parse *pParse, Expr *pExpr);
SQLITE_PRIVATE Expr *sqlite3ExprSetColl(Parse *pParse, Expr *, Token *);
SQLITE_PRIVATE int sqlite3CheckCollSeq(Parse *, CollSeq *);
SQLITE_PRIVATE int sqlite3CheckObjectName(Parse *, const char *);
SQLITE_PRIVATE void sqlite3VdbeSetChanges(sqlite3 *, int);

SQLITE_PRIVATE const void *sqlite3ValueText(sqlite3_value*, u8);
SQLITE_PRIVATE int sqlite3ValueBytes(sqlite3_value*, u8);
SQLITE_PRIVATE void sqlite3ValueSetStr(sqlite3_value*, int, const void *,u8, void(*)(void*));
SQLITE_PRIVATE void sqlite3ValueFree(sqlite3_value*);
SQLITE_PRIVATE sqlite3_value *sqlite3ValueNew(void);
SQLITE_PRIVATE char *sqlite3Utf16to8(const void*, int);
SQLITE_PRIVATE int sqlite3ValueFromExpr(Expr *, u8, u8, sqlite3_value **);
SQLITE_PRIVATE void sqlite3ValueApplyAffinity(sqlite3_value *, u8, u8);
SQLITE_PRIVATE const unsigned char sqlite3UpperToLower[];
SQLITE_PRIVATE void sqlite3RootPageMoved(Db*, int, int);
SQLITE_PRIVATE void sqlite3Reindex(Parse*, Token*, Token*);
SQLITE_PRIVATE void sqlite3AlterFunctions(sqlite3*);
SQLITE_PRIVATE void sqlite3AlterRenameTable(Parse*, SrcList*, Token*);
SQLITE_PRIVATE int sqlite3GetToken(const unsigned char *, int *);
SQLITE_PRIVATE void sqlite3NestedParse(Parse*, const char*, ...);
SQLITE_PRIVATE void sqlite3ExpirePreparedStatements(sqlite3*);
SQLITE_PRIVATE void sqlite3CodeSubselect(Parse *, Expr *);
SQLITE_PRIVATE int sqlite3SelectResolve(Parse *, Select *, NameContext *);
SQLITE_PRIVATE void sqlite3ColumnDefault(Vdbe *, Table *, int);
SQLITE_PRIVATE void sqlite3AlterFinishAddColumn(Parse *, Token *);
SQLITE_PRIVATE void sqlite3AlterBeginAddColumn(Parse *, SrcList *);
SQLITE_PRIVATE CollSeq *sqlite3GetCollSeq(sqlite3*, CollSeq *, const char *, int);
SQLITE_PRIVATE char sqlite3AffinityType(const Token*);
SQLITE_PRIVATE void sqlite3Analyze(Parse*, Token*, Token*);
SQLITE_PRIVATE int sqlite3InvokeBusyHandler(BusyHandler*);
SQLITE_PRIVATE int sqlite3FindDb(sqlite3*, Token*);
SQLITE_PRIVATE int sqlite3AnalysisLoad(sqlite3*,int iDB);
SQLITE_PRIVATE void sqlite3DefaultRowEst(Index*);
SQLITE_PRIVATE void sqlite3RegisterLikeFunctions(sqlite3*, int);
SQLITE_PRIVATE int sqlite3IsLikeFunction(sqlite3*,Expr*,int*,char*);
SQLITE_PRIVATE ThreadData *sqlite3ThreadData(void);
SQLITE_PRIVATE const ThreadData *sqlite3ThreadDataReadOnly(void);
SQLITE_PRIVATE void sqlite3ReleaseThreadData(void);
SQLITE_PRIVATE void sqlite3AttachFunctions(sqlite3 *);
SQLITE_PRIVATE void sqlite3MinimumFileFormat(Parse*, int, int);
SQLITE_PRIVATE void sqlite3SchemaFree(void *);
SQLITE_PRIVATE Schema *sqlite3SchemaGet(Btree *);
SQLITE_PRIVATE int sqlite3SchemaToIndex(sqlite3 *db, Schema *);
SQLITE_PRIVATE KeyInfo *sqlite3IndexKeyinfo(Parse *, Index *);
SQLITE_PRIVATE int sqlite3CreateFunc(sqlite3 *, const char *, int, int, void *, 
  void (*)(sqlite3_context*,int,sqlite3_value **),
  void (*)(sqlite3_context*,int,sqlite3_value **), void (*)(sqlite3_context*));
SQLITE_PRIVATE int sqlite3ApiExit(sqlite3 *db, int);
SQLITE_PRIVATE void sqlite3FailedMalloc(void);
SQLITE_PRIVATE void sqlite3AbortOtherActiveVdbes(sqlite3 *, Vdbe *);
SQLITE_PRIVATE int sqlite3OpenTempDatabase(Parse *);

/*
** The interface to the LEMON-generated parser
*/
SQLITE_PRIVATE void *sqlite3ParserAlloc(void*(*)(size_t));
SQLITE_PRIVATE void sqlite3ParserFree(void*, void(*)(void*));
SQLITE_PRIVATE void sqlite3Parser(void*, int, Token, Parse*);

#ifndef SQLITE_OMIT_LOAD_EXTENSION
SQLITE_PRIVATE   void sqlite3CloseExtensions(sqlite3*);
SQLITE_PRIVATE   int sqlite3AutoLoadExtensions(sqlite3*);
#else
# define sqlite3CloseExtensions(X)
# define sqlite3AutoLoadExtensions(X)  SQLITE_OK
#endif

#ifndef SQLITE_OMIT_SHARED_CACHE
SQLITE_PRIVATE   void sqlite3TableLock(Parse *, int, int, u8, const char *);
#else
  #define sqlite3TableLock(v,w,x,y,z)
#endif

#ifdef SQLITE_TEST
SQLITE_PRIVATE   int sqlite3Utf8To8(unsigned char*);
#endif

#ifdef SQLITE_MEMDEBUG
SQLITE_PRIVATE   void sqlite3MallocDisallow(void);
SQLITE_PRIVATE   void sqlite3MallocAllow(void);
SQLITE_PRIVATE   int sqlite3TestMallocFail(void);
#else
  #define sqlite3TestMallocFail() 0
  #define sqlite3MallocDisallow()
  #define sqlite3MallocAllow()
#endif

#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT
SQLITE_PRIVATE   void *sqlite3ThreadSafeMalloc(int);
SQLITE_PRIVATE   void sqlite3ThreadSafeFree(void *);
#else
  #define sqlite3ThreadSafeMalloc sqlite3MallocX
  #define sqlite3ThreadSafeFree sqlite3FreeX
#endif

#ifdef SQLITE_OMIT_VIRTUALTABLE
#  define sqlite3VtabClear(X)
#  define sqlite3VtabSync(X,Y) (Y)
#  define sqlite3VtabRollback(X)
#  define sqlite3VtabCommit(X)
#else
SQLITE_PRIVATE    void sqlite3VtabClear(Table*);
SQLITE_PRIVATE    int sqlite3VtabSync(sqlite3 *db, int rc);
SQLITE_PRIVATE    int sqlite3VtabRollback(sqlite3 *db);
SQLITE_PRIVATE    int sqlite3VtabCommit(sqlite3 *db);
#endif
SQLITE_PRIVATE void sqlite3VtabLock(sqlite3_vtab*);
SQLITE_PRIVATE void sqlite3VtabUnlock(sqlite3*, sqlite3_vtab*);
SQLITE_PRIVATE void sqlite3VtabBeginParse(Parse*, Token*, Token*, Token*);
SQLITE_PRIVATE void sqlite3VtabFinishParse(Parse*, Token*);
SQLITE_PRIVATE void sqlite3VtabArgInit(Parse*);
SQLITE_PRIVATE void sqlite3VtabArgExtend(Parse*, Token*);
SQLITE_PRIVATE int sqlite3VtabCallCreate(sqlite3*, int, const char *, char **);
SQLITE_PRIVATE int sqlite3VtabCallConnect(Parse*, Table*);
SQLITE_PRIVATE int sqlite3VtabCallDestroy(sqlite3*, int, const char *);
SQLITE_PRIVATE int sqlite3VtabBegin(sqlite3 *, sqlite3_vtab *);
SQLITE_PRIVATE FuncDef *sqlite3VtabOverloadFunction(FuncDef*, int nArg, Expr*);
SQLITE_PRIVATE void sqlite3InvalidFunction(sqlite3_context*,int,sqlite3_value**);
SQLITE_PRIVATE int sqlite3Reprepare(Vdbe*);
SQLITE_PRIVATE void sqlite3ExprListCheckLength(Parse*, ExprList*, int, const char*);
SQLITE_PRIVATE CollSeq *sqlite3BinaryCompareCollSeq(Parse *, Expr *, Expr *);

#if SQLITE_MAX_EXPR_DEPTH>0
SQLITE_PRIVATE   void sqlite3ExprSetHeight(Expr *);
SQLITE_PRIVATE   int sqlite3SelectExprHeight(Select *);
#else
  #define sqlite3ExprSetHeight(x)
#endif

SQLITE_PRIVATE u32 sqlite3Get4byte(const u8*);
SQLITE_PRIVATE void sqlite3Put4byte(u8*, u32);

#ifdef SQLITE_SSE
#include "sseInt.h"
#endif

#ifdef SQLITE_DEBUG
SQLITE_PRIVATE   void sqlite3ParserTrace(FILE*, char *);
#endif

/*
** If the SQLITE_ENABLE IOTRACE exists then the global variable
** sqlite3_io_trace is a pointer to a printf-like routine used to
** print I/O tracing messages. 
*/
#ifdef SQLITE_ENABLE_IOTRACE
# define IOTRACE(A)  if( sqlite3_io_trace ){ sqlite3_io_trace A; }
SQLITE_PRIVATE   void sqlite3VdbeIOTraceSql(Vdbe*);
#else
# define IOTRACE(A)
# define sqlite3VdbeIOTraceSql(X)
#endif
SQLITE_EXTERN void (*sqlite3_io_trace)(const char*,...);

#endif

/************** End of sqliteInt.h *******************************************/
