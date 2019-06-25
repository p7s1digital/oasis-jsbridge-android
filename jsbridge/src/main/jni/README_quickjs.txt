To add/update QuickJS (tested with quickjs 2019-07-21):
$ download source code from https://bellard.org/quickjs/
$ cd quickjs-XXXX-XX-XX
- copy cutils.c, libregexp-opcode.h, libregexp.h, libunicode.c, list.h, quickjs-opcode.h,
quickjs.h, cutils.h, libregexp.c, libunicode-table.h, libunicode.h, quickjs-atom.h, quickjs.c
into jsbridge/quickjs/jni/quickjs
- update QuickJS version in jsbridge/CMakelists.txt (TODO: read it from the version file)
- apply the following change in quickjs.h/.c:


--- official quickjs.h	2019-07-28 17:03:03.000000000 +0200
+++ adjusted quickjs.h	2019-07-29 22:43:46.000000000 +0200
@@ -554,7 +554,7 @@
 static inline void JS_FreeValue(JSContext *ctx, JSValue v)
 {
     if (JS_VALUE_HAS_REF_COUNT(v)) {
-        JSRefCountHeader *p = JS_VALUE_GET_PTR(v);
+        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(v);
         if (--p->ref_count <= 0) {
             __JS_FreeValue(ctx, v);
         }
@@ -564,7 +564,7 @@
 static inline void JS_FreeValueRT(JSRuntime *rt, JSValue v)
 {
     if (JS_VALUE_HAS_REF_COUNT(v)) {
-        JSRefCountHeader *p = JS_VALUE_GET_PTR(v);
+        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(v);
         if (--p->ref_count <= 0) {
             __JS_FreeValueRT(rt, v);
         }
@@ -574,7 +574,7 @@
 static inline JSValue JS_DupValue(JSContext *ctx, JSValueConst v)
 {
     if (JS_VALUE_HAS_REF_COUNT(v)) {
-        JSRefCountHeader *p = JS_VALUE_GET_PTR(v);
+        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(v);
         p->ref_count++;
     }
     return (JSValue)v;
@@ -583,7 +583,7 @@
 static inline JSValue JS_DupValueRT(JSRuntime *rt, JSValueConst v)
 {
     if (JS_VALUE_HAS_REF_COUNT(v)) {
-        JSRefCountHeader *p = JS_VALUE_GET_PTR(v);
+        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(v);
         p->ref_count++;
     }
     return (JSValue)v;


--- official quickjs.c	2019-07-28 17:03:03.000000000 +0200
+++ adjusted quickjs.c	2019-08-09 23:35:09.000000000 +0200
@@ -1292,7 +1292,7 @@
 #elif defined(EMSCRIPTEN)
     return 0;
 #elif defined(__linux__)
-    return malloc_usable_size(ptr);
+    return 0;//malloc_usable_size(ptr);
 #else
     /* change this to `return 0;` if compilation fails */
     return malloc_usable_size(ptr);
@@ -1366,7 +1366,7 @@
 #elif defined(EMSCRIPTEN)
     NULL,
 #elif defined(__linux__)
-    (size_t (*)(const void *))malloc_usable_size,
+    NULL,//(size_t (*)(const void *))malloc_usable_size,
 #else
     /* change this to `NULL,` if compilation fails */
     malloc_usable_size,
@@ -1586,7 +1586,7 @@
             printf("Secondary object leaks: %d\n", count);
     }
 #endif
-    assert(list_empty(&rt->obj_list));
+    //TODO: assert(list_empty(&rt->obj_list));
 
     /* free the classes */
     for(i = 0; i < rt->class_count; i++) {
@@ -1734,7 +1734,8 @@
 {
     size_t size;
     size = ctx->stack_top - js_get_stack_pointer();
-    return unlikely((size + alloca_size) > ctx->stack_size);
+    //return unlikely((size + alloca_size) > ctx->stack_size);
+    return FALSE;
 }
 #endif

