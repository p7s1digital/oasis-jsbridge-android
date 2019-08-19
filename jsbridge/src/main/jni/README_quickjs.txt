To add/update QuickJS (tested with quickjs 2019-07-21):
$ download source code from https://bellard.org/quickjs/
$ cd quickjs-XXXX-XX-XX
- copy cutils.c, libregexp-opcode.h, libregexp.h, libunicode.c, list.h, quickjs-opcode.h,
quickjs.h, cutils.h, libregexp.c, libunicode-table.h, libunicode.h, quickjs-atom.h, quickjs.c
into jsbridge/quickjs/jni/quickjs
- update QuickJS version in jsbridge/CMakelists.txt (TODO: read it from the version file)
- apply the following change in quickjs.c:

--- original quickjs.c	2019-08-18 11:50:45.000000000 +0200
+++ adjusted quickjs.c	2019-08-19 17:26:11.000000000 +0200
@@ -1343,7 +1343,7 @@
 #elif defined(EMSCRIPTEN)
     return 0;
 #elif defined(__linux__)
-    return malloc_usable_size(ptr);
+    return 0;  //malloc_usable_size(ptr);
 #else
     /* change this to `return 0;` if compilation fails */
     return malloc_usable_size(ptr);
@@ -1417,7 +1417,7 @@
 #elif defined(EMSCRIPTEN)
     NULL,
 #elif defined(__linux__)
-    (size_t (*)(const void *))malloc_usable_size,
+    NULL,  //(size_t (*)(const void *))malloc_usable_size,
 #else
     /* change this to `NULL,` if compilation fails */
     malloc_usable_size,
@@ -1637,7 +1637,7 @@
             printf("Secondary object leaks: %d\n", count);
     }
 #endif
-    assert(list_empty(&rt->obj_list));
+    //assert(list_empty(&rt->obj_list));
 
     /* free the classes */
     for(i = 0; i < rt->class_count; i++) {
@@ -1769,7 +1769,7 @@
     }
 }
 
-#if defined(EMSCRIPTEN)
+#if 1  //defined(EMSCRIPTEN)
 /* currently no stack limitation */
 static inline uint8_t *js_get_stack_pointer(void)
 {

