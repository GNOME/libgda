/*
** Copyright (c) 2001 D. Richard Hipp
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public
** License as published by the Free Software Foundation; either
** version 2 of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
** 
** You should have received a copy of the GNU General Public
** License along with this library; if not, write to the
** Free Software Foundation, Inc., 59 Temple Place - Suite 330,
** Boston, MA  02111-1307, USA.
**
** Author contact information:
**   drh@hwaci.com
**   http://www.hwaci.com/drh/
**
*************************************************************************
** Code for testing the printf() interface to SQLite.  This code
** is not included in the SQLite library.  It is used in the regression
** test only.
**
** $Id$
*/
#include "sqlite.h"
#include "tcl.h"
#include <stdlib.h>
#include <string.h>

/*
** Usage:   sqlite_open filename
**
** Returns:  The name of an open database.
*/
static int sqlite_test_open(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  char **argv            /* Text of each argument */
){
  sqlite *db;
  char *zErr = 0;
  char zBuf[100];
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " FILENAME\"", 0);
    return TCL_ERROR;
  }
  db = sqlite_open(argv[1], 0666, &zErr);
  if( db==0 ){
    Tcl_AppendResult(interp, zErr, 0);
    free(zErr);
    return TCL_ERROR;
  }
  sprintf(zBuf,"%d",(int)db);
  Tcl_AppendResult(interp, zBuf, 0);
  return TCL_OK;
}

/*
** Usage:  sqlite_close DB
**
** Closes the database opened by sqlite_open.
*/
static int sqlite_test_close(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  char **argv            /* Text of each argument */
){
  sqlite *db;
  char *zErr = 0;
  char zBuf[100];
  if( argc!=2 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " FILENAME\"", 0);
    return TCL_ERROR;
  }
  db = (sqlite*)atoi(argv[1]);
  sqlite_close(db);
  return TCL_OK;
}

/*
** Usage:  sqlite_mprintf_int FORMAT INTEGER INTEGER INTEGER
**
** Call mprintf with three integer arguments
*/
static int sqlite_mprintf_int(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  char **argv            /* Text of each argument */
){
  int a[3], i;
  char *z;
  if( argc!=5 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " FORMAT INT INT INT\"", 0);
    return TCL_ERROR;
  }
  for(i=2; i<=5; i++){
    if( Tcl_GetInt(interp, argv[i], &a[i-2]) ) return TCL_ERROR;
  }
  z = mprintf(argv[1], a[0], a[1], a[2]);
  Tcl_AppendResult(interp, z, 0);
  free(z);
  return TCL_OK;
}

/*
** Usage:  sqlite_mprintf_str FORMAT INTEGER INTEGER STRING
**
** Call mprintf with two integer arguments and one string argument
*/
static int sqlite_mprintf_int(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  char **argv            /* Text of each argument */
){
  int a[3], i;
  char *z;
  if( argc!=5 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " FORMAT INT INT STRING\"", 0);
    return TCL_ERROR;
  }
  for(i=2; i<=4; i++){
    if( Tcl_GetInt(interp, argv[i], &a[i-2]) ) return TCL_ERROR;
  }
  z = mprintf(argv[1], a[0], a[1], argv[5]);
  Tcl_AppendResult(interp, z, 0);
  free(z);
  return TCL_OK;
}

/*
** Usage:  sqlite_mprintf_str FORMAT INTEGER INTEGER DOUBLE
**
** Call mprintf with two integer arguments and one double argument
*/
static int sqlite_mprintf_double(
  void *NotUsed,
  Tcl_Interp *interp,    /* The TCL interpreter that invoked this command */
  int argc,              /* Number of arguments */
  char **argv            /* Text of each argument */
){
  int a[3], i;
  double r;
  char *z;
  if( argc!=5 ){
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
       " FORMAT INT INT STRING\"", 0);
    return TCL_ERROR;
  }
  for(i=2; i<=4; i++){
    if( Tcl_GetInt(interp, argv[i], &a[i-2]) ) return TCL_ERROR;
  }
  if( Tcl_GetDouble(interp, argv[5], &r) ) return TCL_ERROR;
  z = mprintf(argv[1], a[0], a[1], r);
  Tcl_AppendResult(interp, z, 0);
  free(z);
  return TCL_OK;
}

/*
** Register commands with the TCL interpreter.
*/
int Sqlitetest_Init(Tcl_Interp *interp){
  Tcl_CreateCommand(interp, "sqlite_mprintf_int", sqlite_mprintf_int, 0, 0);
  Tcl_CreateCommand(interp, "sqlite_mprintf_str", sqlite_mprintf_str, 0, 0);
  Tcl_CreateCommand(interp, "sqlite_mprintf_double", sqlite_mprintf_double,0,0);
  Tcl_CreateCommand(interp, "sqlite_open", sqlite_test_open, 0, 0);
  Tcl_CreateCommand(interp, "sqlite_close", sqlite_test_close, 0, 0);
  return TCL_OK;
}
