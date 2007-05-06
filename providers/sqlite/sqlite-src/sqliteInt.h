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
void sqlite3HashInit(Hash*, int keytype, int copyKey);
void *sqlite3HashInsert(Hash*, const void *pKey, int nKey, void *pData);
void *sqlite3HashFind(const Hash*, const void *pKey, int nKey);
void sqlite3HashClear(Hash*);

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
#endif
#ifndef SQLITE_BIG_DBL
# define SQLITE_BIG_DBL (1e99)
#endif

/*
** The maximum number of in-memory pages to use for the main database
** table and for temporary tables. Internally, the MAX_PAGES and 
** TEMP_PAGES macros are used. To override the default values at
** compilation time, the SQLITE_DEFAULT_CACHE_SIZE and 
** SQLITE_DEFAULT_TEMP_CACHE_SIZE macros should be set.
*/
#ifdef SQLITE_DEFAULT_CACHE_SIZE
# define MAX_PAGES SQLITE_DEFAULT_CACHE_SIZE
#else
# define MAX_PAGES   2000
#endif
#ifdef SQLITE_DEFAULT_TEMP_CACHE_SIZE
# define TEMP_PAGES SQLITE_DEFAULT_TEMP_CACHE_SIZE
#else
# define TEMP_PAGES   500
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
** The maximum number of attached databases.  This must be at least 2
** in order to support the main database file (0) and the file used to
** hold temporary tables (1).  And it must be less than 32 because
** we use a bitmask of databases with a u32 in places (for example
** the Parse.cookieMask field).
*/
#define MAX_ATTACHED 10

/*
** The maximum value of a ?nnn wildcard that the parser will accept.
*/
#define SQLITE_MAX_VARIABLE_NUMBER 999

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
extern const int sqlite3one;
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
#define OP_ParseSchema                         28
#define OP_VOpen                               29
#define OP_Close                               30
#define OP_CreateIndex                         31
#define OP_IsUnique                            32
#define OP_NotFound                            33
#define OP_Int64                               34
#define OP_MustBeInt                           35
#define OP_Halt                                36
#define OP_Rowid                               37
#define OP_IdxLT                               38
#define OP_AddImm                              39
#define OP_Statement                           40
#define OP_RowData                             41
#define OP_MemMax                              42
#define OP_Push                                43
#define OP_Or                                  60   /* same as TK_OR       */
#define OP_NotExists                           44
#define OP_MemIncr                             45
#define OP_Gosub                               46
#define OP_Divide                              81   /* same as TK_SLASH    */
#define OP_Integer                             47
#define OP_ToNumeric                          140   /* same as TK_TO_NUMERIC*/
#define OP_MemInt                              48
#define OP_Prev                                49
#define OP_Concat                              83   /* same as TK_CONCAT   */
#define OP_BitAnd                              74   /* same as TK_BITAND   */
#define OP_VColumn                             50
#define OP_CreateTable                         51
#define OP_Last                                52
#define OP_IsNull                              65   /* same as TK_ISNULL   */
#define OP_IdxRowid                            53
#define OP_MakeIdxRec                          54
#define OP_ShiftRight                          77   /* same as TK_RSHIFT   */
#define OP_ResetCount                          55
#define OP_FifoWrite                           56
#define OP_Callback                            57
#define OP_ContextPush                         58
#define OP_DropTrigger                         59
#define OP_DropIndex                           62
#define OP_IdxGE                               63
#define OP_IdxDelete                           64
#define OP_Vacuum                              73
#define OP_MoveLe                              84
#define OP_IfNot                               86
#define OP_DropTable                           89
#define OP_MakeRecord                          90
#define OP_ToBlob                             139   /* same as TK_TO_BLOB  */
#define OP_Delete                              91
#define OP_AggFinal                            92
#define OP_ShiftLeft                           76   /* same as TK_LSHIFT   */
#define OP_Dup                                 93
#define OP_Goto                                94
#define OP_TableLock                           95
#define OP_FifoRead                            96
#define OP_Clear                               97
#define OP_IdxGT                               98
#define OP_MoveLt                              99
#define OP_Le                                  70   /* same as TK_LE       */
#define OP_VerifyCookie                       100
#define OP_AggStep                            101
#define OP_Pull                               102
#define OP_ToText                             138   /* same as TK_TO_TEXT  */
#define OP_Not                                 16   /* same as TK_NOT      */
#define OP_ToReal                             142   /* same as TK_TO_REAL  */
#define OP_SetNumColumns                      103
#define OP_AbsValue                           104
#define OP_Transaction                        105
#define OP_VFilter                            106
#define OP_Negative                            85   /* same as TK_UMINUS   */
#define OP_Ne                                  67   /* same as TK_NE       */
#define OP_VDestroy                           107
#define OP_ContextPop                         108
#define OP_BitOr                               75   /* same as TK_BITOR    */
#define OP_Next                               109
#define OP_IdxInsert                          110
#define OP_Distinct                           111
#define OP_Lt                                  71   /* same as TK_LT       */
#define OP_Insert                             112
#define OP_Destroy                            113
#define OP_ReadCookie                         114
#define OP_ForceInt                           115
#define OP_LoadAnalysis                       116
#define OP_Explain                            117
#define OP_IfMemZero                          118
#define OP_OpenPseudo                         119
#define OP_OpenEphemeral                      120
#define OP_Null                               121
#define OP_Blob                               122
#define OP_Add                                 78   /* same as TK_PLUS     */
#define OP_MemStore                           123
#define OP_Rewind                             124
#define OP_MoveGe                             127
#define OP_VBegin                             128
#define OP_VUpdate                            129
#define OP_BitNot                              87   /* same as TK_BITNOT   */
#define OP_VCreate                            130
#define OP_MemMove                            131
#define OP_MemNull                            132
#define OP_Found                              133
#define OP_NullRow                            134

/* The following opcode values are never used */
#define OP_NotUsed_135                        135
#define OP_NotUsed_136                        136
#define OP_NotUsed_137                        137

/* Opcodes that are guaranteed to never push a value onto the stack
** contain a 1 their corresponding position of the following mask
** set.  See the opcodeNoPush() function in vdbeaux.c  */
#define NOPUSH_MASK_0 0xeeb4
#define NOPUSH_MASK_1 0x796b
#define NOPUSH_MASK_2 0x7ddb
#define NOPUSH_MASK_3 0xff92
#define NOPUSH_MASK_4 0xffff
#define NOPUSH_MASK_5 0xdaf7
#define NOPUSH_MASK_6 0xfefe
#define NOPUSH_MASK_7 0x99d9
#define NOPUSH_MASK_8 0x7c67
#define NOPUSH_MASK_9 0x0000

/************** End of opcodes.h *********************************************/
/************** Continuing where we left off in vdbe.h ***********************/

/*
** Prototypes for the VDBE interface.  See comments on the implementation
** for a description of what each of these routines does.
*/
Vdbe *sqlite3VdbeCreate(sqlite3*);
void sqlite3VdbeCreateCallback(Vdbe*, int*);
int sqlite3VdbeAddOp(Vdbe*,int,int,int);
int sqlite3VdbeOp3(Vdbe*,int,int,int,const char *zP3,int);
int sqlite3VdbeAddOpList(Vdbe*, int nOp, VdbeOpList const *aOp);
void sqlite3VdbeChangeP1(Vdbe*, int addr, int P1);
void sqlite3VdbeChangeP2(Vdbe*, int addr, int P2);
void sqlite3VdbeJumpHere(Vdbe*, int addr);
void sqlite3VdbeChangeToNoop(Vdbe*, int addr, int N);
void sqlite3VdbeChangeP3(Vdbe*, int addr, const char *zP1, int N);
VdbeOp *sqlite3VdbeGetOp(Vdbe*, int);
int sqlite3VdbeMakeLabel(Vdbe*);
void sqlite3VdbeDelete(Vdbe*);
void sqlite3VdbeMakeReady(Vdbe*,int,int,int,int);
int sqlite3VdbeFinalize(Vdbe*);
void sqlite3VdbeResolveLabel(Vdbe*, int);
int sqlite3VdbeCurrentAddr(Vdbe*);
void sqlite3VdbeTrace(Vdbe*,FILE*);
void sqlite3VdbeResetStepResult(Vdbe*);
int sqlite3VdbeReset(Vdbe*);
int sqliteVdbeSetVariables(Vdbe*,int,const char**);
void sqlite3VdbeSetNumCols(Vdbe*,int);
int sqlite3VdbeSetColName(Vdbe*, int, int, const char *, int);
void sqlite3VdbeCountChanges(Vdbe*);
sqlite3 *sqlite3VdbeDb(Vdbe*);
void sqlite3VdbeSetSql(Vdbe*, const char *z, int n);
const char *sqlite3VdbeGetSql(Vdbe*);
void sqlite3VdbeSwap(Vdbe*,Vdbe*);

#ifndef NDEBUG
  void sqlite3VdbeComment(Vdbe*, const char*, ...);
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

/*
** Forward declarations of structure
*/
typedef struct Btree Btree;
typedef struct BtCursor BtCursor;
typedef struct BtShared BtShared;


int sqlite3BtreeOpen(
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

int sqlite3BtreeClose(Btree*);
int sqlite3BtreeSetBusyHandler(Btree*,BusyHandler*);
int sqlite3BtreeSetCacheSize(Btree*,int);
int sqlite3BtreeSetSafetyLevel(Btree*,int,int);
int sqlite3BtreeSyncDisabled(Btree*);
int sqlite3BtreeSetPageSize(Btree*,int,int);
int sqlite3BtreeGetPageSize(Btree*);
int sqlite3BtreeGetReserve(Btree*);
int sqlite3BtreeSetAutoVacuum(Btree *, int);
int sqlite3BtreeGetAutoVacuum(Btree *);
int sqlite3BtreeBeginTrans(Btree*,int);
int sqlite3BtreeCommitPhaseOne(Btree*, const char *zMaster);
int sqlite3BtreeCommitPhaseTwo(Btree*);
int sqlite3BtreeCommit(Btree*);
int sqlite3BtreeRollback(Btree*);
int sqlite3BtreeBeginStmt(Btree*);
int sqlite3BtreeCommitStmt(Btree*);
int sqlite3BtreeRollbackStmt(Btree*);
int sqlite3BtreeCreateTable(Btree*, int*, int flags);
int sqlite3BtreeIsInTrans(Btree*);
int sqlite3BtreeIsInStmt(Btree*);
int sqlite3BtreeIsInReadTrans(Btree*);
void *sqlite3BtreeSchema(Btree *, int, void(*)(void *));
int sqlite3BtreeSchemaLocked(Btree *);
int sqlite3BtreeLockTable(Btree *, int, u8);

const char *sqlite3BtreeGetFilename(Btree *);
const char *sqlite3BtreeGetDirname(Btree *);
const char *sqlite3BtreeGetJournalname(Btree *);
int sqlite3BtreeCopyFile(Btree *, Btree *);

/* The flags parameter to sqlite3BtreeCreateTable can be the bitwise OR
** of the following flags:
*/
#define BTREE_INTKEY     1    /* Table has only 64-bit signed integer keys */
#define BTREE_ZERODATA   2    /* Table has keys only - no data */
#define BTREE_LEAFDATA   4    /* Data stored in leaves only.  Implies INTKEY */

int sqlite3BtreeDropTable(Btree*, int, int*);
int sqlite3BtreeClearTable(Btree*, int);
int sqlite3BtreeGetMeta(Btree*, int idx, u32 *pValue);
int sqlite3BtreeUpdateMeta(Btree*, int idx, u32 value);

int sqlite3BtreeCursor(
  Btree*,                              /* BTree containing table to open */
  int iTable,                          /* Index of root page */
  int wrFlag,                          /* 1 for writing.  0 for read-only */
  int(*)(void*,int,const void*,int,const void*),  /* Key comparison function */
  void*,                               /* First argument to compare function */
  BtCursor **ppCursor                  /* Returned cursor */
);

void sqlite3BtreeSetCompare(
  BtCursor *,
  int(*)(void*,int,const void*,int,const void*),
  void*
);

int sqlite3BtreeCloseCursor(BtCursor*);
int sqlite3BtreeMoveto(BtCursor*,const void *pKey,i64 nKey,int bias,int *pRes);
int sqlite3BtreeDelete(BtCursor*);
int sqlite3BtreeInsert(BtCursor*, const void *pKey, i64 nKey,
                                  const void *pData, int nData, int bias);
int sqlite3BtreeFirst(BtCursor*, int *pRes);
int sqlite3BtreeLast(BtCursor*, int *pRes);
int sqlite3BtreeNext(BtCursor*, int *pRes);
int sqlite3BtreeEof(BtCursor*);
int sqlite3BtreeFlags(BtCursor*);
int sqlite3BtreePrevious(BtCursor*, int *pRes);
int sqlite3BtreeKeySize(BtCursor*, i64 *pSize);
int sqlite3BtreeKey(BtCursor*, u32 offset, u32 amt, void*);
const void *sqlite3BtreeKeyFetch(BtCursor*, int *pAmt);
const void *sqlite3BtreeDataFetch(BtCursor*, int *pAmt);
int sqlite3BtreeDataSize(BtCursor*, u32 *pSize);
int sqlite3BtreeData(BtCursor*, u32 offset, u32 amt, void*);

char *sqlite3BtreeIntegrityCheck(Btree*, int *aRoot, int nRoot, int, int*);
struct Pager *sqlite3BtreePager(Btree*);


#ifdef SQLITE_TEST
int sqlite3BtreeCursorInfo(BtCursor*, int*, int);
void sqlite3BtreeCursorList(Btree*);
#endif

#ifdef SQLITE_DEBUG
int sqlite3BtreePageDump(Btree*, int, int recursive);
#else
#define sqlite3BtreePageDump(X,Y,Z) SQLITE_OK
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
** The default size of a database page.
*/
#ifndef SQLITE_DEFAULT_PAGE_SIZE
# define SQLITE_DEFAULT_PAGE_SIZE 1024
#endif

/* Maximum page size.  The upper bound on this value is 32768.  This a limit
** imposed by necessity of storing the value in a 2-byte unsigned integer
** and the fact that the page size must be a power of 2.
**
** This value is used to initialize certain arrays on the stack at
** various places in the code.  On embedded machines where stack space
** is limited and the flexibility of having large pages is not needed,
** it makes good sense to reduce the maximum page size to something more
** reasonable, like 1024.
*/
#ifndef SQLITE_MAX_PAGE_SIZE
# define SQLITE_MAX_PAGE_SIZE 32768
#endif

/*
** Maximum number of pages in one database.
*/
#define SQLITE_MAX_PAGE 1073741823

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
int sqlite3PagerOpen(Pager **ppPager, const char *zFilename,
                     int nExtra, int flags);
void sqlite3PagerSetBusyhandler(Pager*, BusyHandler *pBusyHandler);
void sqlite3PagerSetDestructor(Pager*, void(*)(DbPage*,int));
void sqlite3PagerSetReiniter(Pager*, void(*)(DbPage*,int));
int sqlite3PagerSetPagesize(Pager*, int);
int sqlite3PagerReadFileheader(Pager*, int, unsigned char*);
void sqlite3PagerSetCachesize(Pager*, int);
int sqlite3PagerClose(Pager *pPager);
int sqlite3PagerAcquire(Pager *pPager, Pgno pgno, DbPage **ppPage, int clrFlag);
#define sqlite3PagerGet(A,B,C) sqlite3PagerAcquire(A,B,C,0)
DbPage *sqlite3PagerLookup(Pager *pPager, Pgno pgno);
int sqlite3PagerRef(DbPage*);
int sqlite3PagerUnref(DbPage*);
Pgno sqlite3PagerPagenumber(DbPage*);
int sqlite3PagerWrite(DbPage*);
int sqlite3PagerIswriteable(DbPage*);
int sqlite3PagerOverwrite(Pager *pPager, Pgno pgno, void*);
int sqlite3PagerPagecount(Pager*);
int sqlite3PagerTruncate(Pager*,Pgno);
int sqlite3PagerBegin(DbPage*, int exFlag);
int sqlite3PagerCommitPhaseOne(Pager*,const char *zMaster, Pgno);
int sqlite3PagerCommitPhaseTwo(Pager*);
int sqlite3PagerRollback(Pager*);
int sqlite3PagerIsreadonly(Pager*);
int sqlite3PagerStmtBegin(Pager*);
int sqlite3PagerStmtCommit(Pager*);
int sqlite3PagerStmtRollback(Pager*);
void sqlite3PagerDontRollback(DbPage*);
void sqlite3PagerDontWrite(DbPage*);
int sqlite3PagerRefcount(Pager*);
int *sqlite3PagerStats(Pager*);
void sqlite3PagerSetSafetyLevel(Pager*,int,int);
const char *sqlite3PagerFilename(Pager*);
const char *sqlite3PagerDirname(Pager*);
const char *sqlite3PagerJournalname(Pager*);
int sqlite3PagerNosync(Pager*);
int sqlite3PagerRename(Pager*, const char *zNewName);
void sqlite3PagerSetCodec(Pager*,void*(*)(void*,void*,Pgno,int),void*);
int sqlite3PagerMovepage(Pager*,DbPage*,Pgno);
int sqlite3PagerReset(Pager*);
int sqlite3PagerReleaseMemory(int);

void *sqlite3PagerGetData(DbPage *); 
void *sqlite3PagerGetExtra(DbPage *); 
int sqlite3PagerLockingMode(Pager *, int);

#if defined(SQLITE_DEBUG) || defined(SQLITE_TEST)
int sqlite3PagerLockstate(Pager*);
#endif

#ifdef SQLITE_TEST
void sqlite3PagerRefdump(Pager*);
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
extern int sqlite3_nMalloc;      /* Number of sqliteMalloc() calls */
extern int sqlite3_nFree;        /* Number of sqliteFree() calls */
extern int sqlite3_iMallocFail;  /* Fail sqliteMalloc() after this many calls */
extern int sqlite3_iMallocReset; /* Set iMallocFail to this when it reaches 0 */

extern void *sqlite3_pFirst;         /* Pointer to linked list of allocations */
extern int sqlite3_nMaxAlloc;        /* High water mark of ThreadData.nAlloc */
extern int sqlite3_mallocDisallowed; /* assert() in sqlite3Malloc() if set */
extern int sqlite3_isFail;           /* True if all malloc calls should fail */
extern const char *sqlite3_zFile;    /* Filename to associate debug info with */
extern int sqlite3_iLine;            /* Line number for debug info */

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
  char *zName;         /* Name of the collating sequence, UTF-8 encoded */
  u8 enc;              /* Text encoding handled by xCmp() */
  u8 type;             /* One of the SQLITE_COLL_... values below */
  void *pUser;         /* First argument to xCmp() */
  int (*xCmp)(void*,int, const void*, int, const void*);
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
#  define IsVirtual(X) ((X)->isVirtual)
#else
#  define IsVirtual(X) 0
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
  u8 parseError;       /* True if a parsing error has been seen */
  int nErr;            /* Number of errors seen */
  int nTab;            /* Number of previously allocated VDBE cursors */
  int nMem;            /* Number of memory cells used so far */
  int nSet;            /* Number of sets used so far */
  int ckOffset;        /* Stack offset to data used by CHECK constraints */
  u32 writeMask;       /* Start a write transaction on these databases */
  u32 cookieMask;      /* Bitmask of schema verified databases */
  int cookieGoto;      /* Address of OP_Goto to cookie verifier subroutine */
  int cookieValue[MAX_ATTACHED+2];  /* Values of cookies to verify */
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
 * This global flag is set for performance testing of triggers. When it is set
 * SQLite will perform the overhead of building new and old trigger references 
 * even when no triggers exist
 */
extern int sqlite3_always_code_trigger_setup;

/*
** The SQLITE_CORRUPT_BKPT macro can be either a constant (for production
** builds) or a function call (for debugging).  If it is a function call,
** it allows the operator to set a breakpoint at the spot where database
** corruption is first detected.
*/
#ifdef SQLITE_DEBUG
  extern int sqlite3Corrupt(void);
# define SQLITE_CORRUPT_BKPT sqlite3Corrupt()
#else
# define SQLITE_CORRUPT_BKPT SQLITE_CORRUPT
#endif

/*
** Internal function prototypes
*/
int sqlite3StrICmp(const char *, const char *);
int sqlite3StrNICmp(const char *, const char *, int);
int sqlite3HashNoCase(const char *, int);
int sqlite3IsNumber(const char*, int*, u8);
int sqlite3Compare(const char *, const char *);
int sqlite3SortCompare(const char *, const char *);
void sqlite3RealToSortable(double r, char *);

void *sqlite3Malloc(int,int);
void *sqlite3MallocRaw(int,int);
void sqlite3Free(void*);
void *sqlite3Realloc(void*,int);
char *sqlite3StrDup(const char*);
char *sqlite3StrNDup(const char*, int);
# define sqlite3CheckMemory(a,b)
void *sqlite3ReallocOrFree(void*,int);
void sqlite3FreeX(void*);
void *sqlite3MallocX(int);
int sqlite3AllocSize(void *);

char *sqlite3MPrintf(const char*, ...);
char *sqlite3VMPrintf(const char*, va_list);
void sqlite3DebugPrintf(const char*, ...);
void *sqlite3TextToPtr(const char*);
void sqlite3SetString(char **, ...);
void sqlite3ErrorMsg(Parse*, const char*, ...);
void sqlite3ErrorClear(Parse*);
void sqlite3Dequote(char*);
void sqlite3DequoteExpr(Expr*);
int sqlite3KeywordCode(const unsigned char*, int);
int sqlite3RunParser(Parse*, const char*, char **);
void sqlite3FinishCoding(Parse*);
Expr *sqlite3Expr(int, Expr*, Expr*, const Token*);
Expr *sqlite3ExprOrFree(int, Expr*, Expr*, const Token*);
Expr *sqlite3RegisterExpr(Parse*,Token*);
Expr *sqlite3ExprAnd(Expr*, Expr*);
void sqlite3ExprSpan(Expr*,Token*,Token*);
Expr *sqlite3ExprFunction(ExprList*, Token*);
void sqlite3ExprAssignVarNumber(Parse*, Expr*);
void sqlite3ExprDelete(Expr*);
ExprList *sqlite3ExprListAppend(ExprList*,Expr*,Token*);
void sqlite3ExprListDelete(ExprList*);
int sqlite3Init(sqlite3*, char**);
int sqlite3InitCallback(void*, int, char**, char**);
void sqlite3Pragma(Parse*,Token*,Token*,Token*,int);
void sqlite3ResetInternalSchema(sqlite3*, int);
void sqlite3BeginParse(Parse*,int);
void sqlite3CommitInternalChanges(sqlite3*);
Table *sqlite3ResultSetOfSelect(Parse*,char*,Select*);
void sqlite3OpenMasterTable(Parse *, int);
void sqlite3StartTable(Parse*,Token*,Token*,int,int,int,int);
void sqlite3AddColumn(Parse*,Token*);
void sqlite3AddNotNull(Parse*, int);
void sqlite3AddPrimaryKey(Parse*, ExprList*, int, int, int);
void sqlite3AddCheckConstraint(Parse*, Expr*);
void sqlite3AddColumnType(Parse*,Token*);
void sqlite3AddDefaultValue(Parse*,Expr*);
void sqlite3AddCollateType(Parse*, const char*, int);
void sqlite3EndTable(Parse*,Token*,Token*,Select*);

void sqlite3CreateView(Parse*,Token*,Token*,Token*,Select*,int,int);

#if !defined(SQLITE_OMIT_VIEW) || !defined(SQLITE_OMIT_VIRTUALTABLE)
  int sqlite3ViewGetColumnNames(Parse*,Table*);
#else
# define sqlite3ViewGetColumnNames(A,B) 0
#endif

void sqlite3DropTable(Parse*, SrcList*, int, int);
void sqlite3DeleteTable(Table*);
void sqlite3Insert(Parse*, SrcList*, ExprList*, Select*, IdList*, int);
void *sqlite3ArrayAllocate(void*,int,int,int*,int*,int*);
IdList *sqlite3IdListAppend(IdList*, Token*);
int sqlite3IdListIndex(IdList*,const char*);
SrcList *sqlite3SrcListAppend(SrcList*, Token*, Token*);
SrcList *sqlite3SrcListAppendFromTerm(SrcList*, Token*, Token*, Token*,
                                      Select*, Expr*, IdList*);
void sqlite3SrcListShiftJoinType(SrcList*);
void sqlite3SrcListAssignCursors(Parse*, SrcList*);
void sqlite3IdListDelete(IdList*);
void sqlite3SrcListDelete(SrcList*);
void sqlite3CreateIndex(Parse*,Token*,Token*,SrcList*,ExprList*,int,Token*,
                        Token*, int, int);
void sqlite3DropIndex(Parse*, SrcList*, int);
void sqlite3AddKeyType(Vdbe*, ExprList*);
void sqlite3AddIdxKeyType(Vdbe*, Index*);
int sqlite3Select(Parse*, Select*, int, int, Select*, int, int*, char *aff);
Select *sqlite3SelectNew(ExprList*,SrcList*,Expr*,ExprList*,Expr*,ExprList*,
                        int,Expr*,Expr*);
void sqlite3SelectDelete(Select*);
void sqlite3SelectUnbind(Select*);
Table *sqlite3SrcListLookup(Parse*, SrcList*);
int sqlite3IsReadOnly(Parse*, Table*, int);
void sqlite3OpenTable(Parse*, int iCur, int iDb, Table*, int);
void sqlite3DeleteFrom(Parse*, SrcList*, Expr*);
void sqlite3Update(Parse*, SrcList*, ExprList*, Expr*, int);
WhereInfo *sqlite3WhereBegin(Parse*, SrcList*, Expr*, ExprList**);
void sqlite3WhereEnd(WhereInfo*);
void sqlite3ExprCodeGetColumn(Vdbe*, Table*, int, int);
void sqlite3ExprCode(Parse*, Expr*);
void sqlite3ExprCodeAndCache(Parse*, Expr*);
int sqlite3ExprCodeExprList(Parse*, ExprList*);
void sqlite3ExprIfTrue(Parse*, Expr*, int, int);
void sqlite3ExprIfFalse(Parse*, Expr*, int, int);
void sqlite3NextedParse(Parse*, const char*, ...);
Table *sqlite3FindTable(sqlite3*,const char*, const char*);
Table *sqlite3LocateTable(Parse*,const char*, const char*);
Index *sqlite3FindIndex(sqlite3*,const char*, const char*);
void sqlite3UnlinkAndDeleteTable(sqlite3*,int,const char*);
void sqlite3UnlinkAndDeleteIndex(sqlite3*,int,const char*);
void sqlite3Vacuum(Parse*);
int sqlite3RunVacuum(char**, sqlite3*);
char *sqlite3NameFromToken(Token*);
int sqlite3ExprCheck(Parse*, Expr*, int, int*);
int sqlite3ExprCompare(Expr*, Expr*);
int sqliteFuncId(Token*);
int sqlite3ExprResolveNames(NameContext *, Expr *);
int sqlite3ExprAnalyzeAggregates(NameContext*, Expr*);
int sqlite3ExprAnalyzeAggList(NameContext*,ExprList*);
Vdbe *sqlite3GetVdbe(Parse*);
Expr *sqlite3CreateIdExpr(const char*);
void sqlite3Randomness(int, void*);
void sqlite3RollbackAll(sqlite3*);
void sqlite3CodeVerifySchema(Parse*, int);
void sqlite3BeginTransaction(Parse*, int);
void sqlite3CommitTransaction(Parse*);
void sqlite3RollbackTransaction(Parse*);
int sqlite3ExprIsConstant(Expr*);
int sqlite3ExprIsConstantOrFunction(Expr*);
int sqlite3ExprIsInteger(Expr*, int*);
int sqlite3IsRowid(const char*);
void sqlite3GenerateRowDelete(sqlite3*, Vdbe*, Table*, int, int);
void sqlite3GenerateRowIndexDelete(Vdbe*, Table*, int, char*);
void sqlite3GenerateIndexKey(Vdbe*, Index*, int);
void sqlite3GenerateConstraintChecks(Parse*,Table*,int,char*,int,int,int,int);
void sqlite3CompleteInsertion(Parse*, Table*, int, char*, int, int, int, int);
void sqlite3OpenTableAndIndices(Parse*, Table*, int, int);
void sqlite3BeginWriteOperation(Parse*, int, int);
Expr *sqlite3ExprDup(Expr*);
void sqlite3TokenCopy(Token*, Token*);
ExprList *sqlite3ExprListDup(ExprList*);
SrcList *sqlite3SrcListDup(SrcList*);
IdList *sqlite3IdListDup(IdList*);
Select *sqlite3SelectDup(Select*);
FuncDef *sqlite3FindFunction(sqlite3*,const char*,int,int,u8,int);
void sqlite3RegisterBuiltinFunctions(sqlite3*);
void sqlite3RegisterDateTimeFunctions(sqlite3*);
int sqlite3SafetyOn(sqlite3*);
int sqlite3SafetyOff(sqlite3*);
int sqlite3SafetyCheck(sqlite3*);
void sqlite3ChangeCookie(sqlite3*, Vdbe*, int);

#ifndef SQLITE_OMIT_TRIGGER
  void sqlite3BeginTrigger(Parse*, Token*,Token*,int,int,IdList*,SrcList*,
                           Expr*,int, int);
  void sqlite3FinishTrigger(Parse*, TriggerStep*, Token*);
  void sqlite3DropTrigger(Parse*, SrcList*, int);
  void sqlite3DropTriggerPtr(Parse*, Trigger*);
  int sqlite3TriggersExist(Parse*, Table*, int, ExprList*);
  int sqlite3CodeRowTrigger(Parse*, int, ExprList*, int, Table *, int, int, 
                           int, int);
  void sqliteViewTriggers(Parse*, Table*, Expr*, int, ExprList*);
  void sqlite3DeleteTriggerStep(TriggerStep*);
  TriggerStep *sqlite3TriggerSelectStep(Select*);
  TriggerStep *sqlite3TriggerInsertStep(Token*, IdList*, ExprList*,Select*,int);
  TriggerStep *sqlite3TriggerUpdateStep(Token*, ExprList*, Expr*, int);
  TriggerStep *sqlite3TriggerDeleteStep(Token*, Expr*);
  void sqlite3DeleteTrigger(Trigger*);
  void sqlite3UnlinkAndDeleteTrigger(sqlite3*,int,const char*);
#else
# define sqlite3TriggersExist(A,B,C,D,E,F) 0
# define sqlite3DeleteTrigger(A)
# define sqlite3DropTriggerPtr(A,B)
# define sqlite3UnlinkAndDeleteTrigger(A,B,C)
# define sqlite3CodeRowTrigger(A,B,C,D,E,F,G,H,I) 0
#endif

int sqlite3JoinType(Parse*, Token*, Token*, Token*);
void sqlite3CreateForeignKey(Parse*, ExprList*, Token*, ExprList*, int);
void sqlite3DeferForeignKey(Parse*, int);
#ifndef SQLITE_OMIT_AUTHORIZATION
  void sqlite3AuthRead(Parse*,Expr*,SrcList*);
  int sqlite3AuthCheck(Parse*,int, const char*, const char*, const char*);
  void sqlite3AuthContextPush(Parse*, AuthContext*, const char*);
  void sqlite3AuthContextPop(AuthContext*);
#else
# define sqlite3AuthRead(a,b,c)
# define sqlite3AuthCheck(a,b,c,d,e)    SQLITE_OK
# define sqlite3AuthContextPush(a,b,c)
# define sqlite3AuthContextPop(a)  ((void)(a))
#endif
void sqlite3Attach(Parse*, Expr*, Expr*, Expr*);
void sqlite3Detach(Parse*, Expr*);
int sqlite3BtreeFactory(const sqlite3 *db, const char *zFilename,
                       int omitJournal, int nCache, Btree **ppBtree);
int sqlite3FixInit(DbFixer*, Parse*, int, const char*, const Token*);
int sqlite3FixSrcList(DbFixer*, SrcList*);
int sqlite3FixSelect(DbFixer*, Select*);
int sqlite3FixExpr(DbFixer*, Expr*);
int sqlite3FixExprList(DbFixer*, ExprList*);
int sqlite3FixTriggerStep(DbFixer*, TriggerStep*);
int sqlite3AtoF(const char *z, double*);
char *sqlite3_snprintf(int,char*,const char*,...);
int sqlite3GetInt32(const char *, int*);
int sqlite3FitsIn64Bits(const char *);
int sqlite3utf16ByteLen(const void *pData, int nChar);
int sqlite3utf8CharLen(const char *pData, int nByte);
int sqlite3ReadUtf8(const unsigned char *);
int sqlite3PutVarint(unsigned char *, u64);
int sqlite3GetVarint(const unsigned char *, u64 *);
int sqlite3GetVarint32(const unsigned char *, u32 *);
int sqlite3VarintLen(u64 v);
void sqlite3IndexAffinityStr(Vdbe *, Index *);
void sqlite3TableAffinityStr(Vdbe *, Table *);
char sqlite3CompareAffinity(Expr *pExpr, char aff2);
int sqlite3IndexAffinityOk(Expr *pExpr, char idx_affinity);
char sqlite3ExprAffinity(Expr *pExpr);
int sqlite3atoi64(const char*, i64*);
void sqlite3Error(sqlite3*, int, const char*,...);
void *sqlite3HexToBlob(const char *z);
int sqlite3TwoPartName(Parse *, Token *, Token *, Token **);
const char *sqlite3ErrStr(int);
int sqlite3ReadUniChar(const char *zStr, int *pOffset, u8 *pEnc, int fold);
int sqlite3ReadSchema(Parse *pParse);
CollSeq *sqlite3FindCollSeq(sqlite3*,u8 enc, const char *,int,int);
CollSeq *sqlite3LocateCollSeq(Parse *pParse, const char *zName, int nName);
CollSeq *sqlite3ExprCollSeq(Parse *pParse, Expr *pExpr);
Expr *sqlite3ExprSetColl(Parse *pParse, Expr *, Token *);
int sqlite3CheckCollSeq(Parse *, CollSeq *);
int sqlite3CheckIndexCollSeq(Parse *, Index *);
int sqlite3CheckObjectName(Parse *, const char *);
void sqlite3VdbeSetChanges(sqlite3 *, int);
void sqlite3utf16Substr(sqlite3_context *,int,sqlite3_value **);

const void *sqlite3ValueText(sqlite3_value*, u8);
int sqlite3ValueBytes(sqlite3_value*, u8);
void sqlite3ValueSetStr(sqlite3_value*, int, const void *,u8, void(*)(void*));
void sqlite3ValueFree(sqlite3_value*);
sqlite3_value *sqlite3ValueNew(void);
char *sqlite3utf16to8(const void*, int);
int sqlite3ValueFromExpr(Expr *, u8, u8, sqlite3_value **);
void sqlite3ValueApplyAffinity(sqlite3_value *, u8, u8);
extern const unsigned char sqlite3UpperToLower[];
void sqlite3RootPageMoved(Db*, int, int);
void sqlite3Reindex(Parse*, Token*, Token*);
void sqlite3AlterFunctions(sqlite3*);
void sqlite3AlterRenameTable(Parse*, SrcList*, Token*);
int sqlite3GetToken(const unsigned char *, int *);
void sqlite3NestedParse(Parse*, const char*, ...);
void sqlite3ExpirePreparedStatements(sqlite3*);
void sqlite3CodeSubselect(Parse *, Expr *);
int sqlite3SelectResolve(Parse *, Select *, NameContext *);
void sqlite3ColumnDefault(Vdbe *, Table *, int);
void sqlite3AlterFinishAddColumn(Parse *, Token *);
void sqlite3AlterBeginAddColumn(Parse *, SrcList *);
const char *sqlite3TestErrorName(int);
CollSeq *sqlite3GetCollSeq(sqlite3*, CollSeq *, const char *, int);
char sqlite3AffinityType(const Token*);
void sqlite3Analyze(Parse*, Token*, Token*);
int sqlite3InvokeBusyHandler(BusyHandler*);
int sqlite3FindDb(sqlite3*, Token*);
void sqlite3AnalysisLoad(sqlite3*,int iDB);
void sqlite3DefaultRowEst(Index*);
void sqlite3RegisterLikeFunctions(sqlite3*, int);
int sqlite3IsLikeFunction(sqlite3*,Expr*,int*,char*);
ThreadData *sqlite3ThreadData(void);
const ThreadData *sqlite3ThreadDataReadOnly(void);
void sqlite3ReleaseThreadData(void);
void sqlite3AttachFunctions(sqlite3 *);
void sqlite3MinimumFileFormat(Parse*, int, int);
void sqlite3SchemaFree(void *);
Schema *sqlite3SchemaGet(Btree *);
int sqlite3SchemaToIndex(sqlite3 *db, Schema *);
KeyInfo *sqlite3IndexKeyinfo(Parse *, Index *);
int sqlite3CreateFunc(sqlite3 *, const char *, int, int, void *, 
  void (*)(sqlite3_context*,int,sqlite3_value **),
  void (*)(sqlite3_context*,int,sqlite3_value **), void (*)(sqlite3_context*));
int sqlite3ApiExit(sqlite3 *db, int);
int sqlite3MallocFailed(void);
void sqlite3FailedMalloc(void);
void sqlite3AbortOtherActiveVdbes(sqlite3 *, Vdbe *);
int sqlite3OpenTempDatabase(Parse *);

#ifndef SQLITE_OMIT_LOAD_EXTENSION
  void sqlite3CloseExtensions(sqlite3*);
  int sqlite3AutoLoadExtensions(sqlite3*);
#else
# define sqlite3CloseExtensions(X)
# define sqlite3AutoLoadExtensions(X)  SQLITE_OK
#endif

#ifndef SQLITE_OMIT_SHARED_CACHE
  void sqlite3TableLock(Parse *, int, int, u8, const char *);
#else
  #define sqlite3TableLock(v,w,x,y,z)
#endif

#ifdef SQLITE_MEMDEBUG
  void sqlite3MallocDisallow(void);
  void sqlite3MallocAllow(void);
  int sqlite3TestMallocFail(void);
#else
  #define sqlite3TestMallocFail() 0
  #define sqlite3MallocDisallow()
  #define sqlite3MallocAllow()
#endif

#ifdef SQLITE_ENABLE_MEMORY_MANAGEMENT
  void *sqlite3ThreadSafeMalloc(int);
  void sqlite3ThreadSafeFree(void *);
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
   void sqlite3VtabClear(Table*);
   int sqlite3VtabSync(sqlite3 *db, int rc);
   int sqlite3VtabRollback(sqlite3 *db);
   int sqlite3VtabCommit(sqlite3 *db);
#endif
void sqlite3VtabLock(sqlite3_vtab*);
void sqlite3VtabUnlock(sqlite3*, sqlite3_vtab*);
void sqlite3VtabBeginParse(Parse*, Token*, Token*, Token*);
void sqlite3VtabFinishParse(Parse*, Token*);
void sqlite3VtabArgInit(Parse*);
void sqlite3VtabArgExtend(Parse*, Token*);
int sqlite3VtabCallCreate(sqlite3*, int, const char *, char **);
int sqlite3VtabCallConnect(Parse*, Table*);
int sqlite3VtabCallDestroy(sqlite3*, int, const char *);
int sqlite3VtabBegin(sqlite3 *, sqlite3_vtab *);
FuncDef *sqlite3VtabOverloadFunction(FuncDef*, int nArg, Expr*);
void sqlite3InvalidFunction(sqlite3_context*,int,sqlite3_value**);
int sqlite3Reprepare(Vdbe*);

#ifdef SQLITE_SSE
#include "sseInt.h"
#endif

/*
** If the SQLITE_ENABLE IOTRACE exists then the global variable
** sqlite3_io_trace is a pointer to a printf-like routine used to
** print I/O tracing messages. 
*/
#ifdef SQLITE_ENABLE_IOTRACE
# define IOTRACE(A)  if( sqlite3_io_trace ){ sqlite3_io_trace A; }
  void sqlite3VdbeIOTraceSql(Vdbe*);
#else
# define IOTRACE(A)
# define sqlite3VdbeIOTraceSql(X)
#endif
extern void (*sqlite3_io_trace)(const char*,...);

#endif

/************** End of sqliteInt.h *******************************************/
