To add/update QuickJS (tested with quickjs 2019-07-21):
$ download source code from https://bellard.org/quickjs/
$ cd quickjs-XXXX-XX-XX
- copy VERSION, cutils.c, libregexp-opcode.h, libregexp.h, libunicode.c, list.h, quickjs-opcode.h,
quickjs.h, cutils.h, libregexp.c, libunicode-table.h, libunicode.h, quickjs-atom.h, quickjs.c
into jsbridge/src/main/jni/quickjs
- apply the following change in quickjs.c:

--- original quickjs.c	2019-08-18 11:50:45.000000000 +0200
+++ adjusted quickjs.c	2019-08-19 17:26:11.000000000 +0200
--- original quickjs.c	2020-09-21 11:31:21.359093775 +0200
+++ adjusted quickjs.c	2020-09-21 13:27:20.199271540 +0200
@@ -71,7 +71,7 @@
 #define CONFIG_ATOMICS
 #endif
 
-#if !defined(EMSCRIPTEN)
+#if 0  //!defined(EMSCRIPTEN)
 /* enable stack limitation */
 #define CONFIG_STACK_CHECK
 #endif

