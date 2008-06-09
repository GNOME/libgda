--- db.h.orig	2008-06-09 14:22:38.000000000 +0200
+++ db.h	2008-06-09 14:24:24.000000000 +0200
@@ -75,17 +75,23 @@
  * We also provide the standard u_int, u_long etc., if they're not provided
  * by the system.
  */
-#ifndef	__BIT_TYPES_DEFINED__
-#define	__BIT_TYPES_DEFINED__
+#ifndef __BIT_TYPES_DEFINED__
+#define __BIT_TYPES_DEFINED__
 typedef unsigned char u_int8_t;
-typedef short int16_t;
 typedef unsigned short u_int16_t;
-typedef int int32_t;
 typedef unsigned int u_int32_t;
-typedef __int64 int64_t;
 typedef unsigned __int64 u_int64_t;
+
+#if defined __GNUC__
+#include <inttypes.h>
+#else
+typedef short int16_t;
+typedef int int32_t;
+typedef __int64 int64_t;
+#endif
 #endif
 
+
 #ifndef _WINSOCKAPI_
 typedef unsigned char u_char;
 typedef unsigned int u_int;
@@ -111,12 +117,14 @@
  * get upset about that.  So far we haven't run on any machine where there's
  * no unsigned type the same size as a pointer -- here's hoping.
  */
+#if !defined __GNUC__
 typedef u_int64_t uintmax_t;
 #ifdef _WIN64
 typedef u_int64_t uintptr_t;
 #else
 typedef u_int32_t uintptr_t;
 #endif
+#endif
 
 /*
  * Windows defines off_t to long (i.e., 32 bits).  We need to pass 64-bit
