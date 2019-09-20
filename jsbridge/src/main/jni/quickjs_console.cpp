// Based on quickjs-libc.c (QuickJS) with the following modifications:
// - forward errors to JS bridge via JNI
// - TODO: fix some formatting issues (including support for Error)

#include "quickjs_console.h"

#include "JniCache.h"
#include "JsBridgeContext.h"
#include "log.h"
#include "jni-helpers/JStringLocalRef.h"
#include "jni-helpers/JniLocalRef.h"
#include <cassert>
#include <sstream>

static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
  const char *logType = "d";  // TODO: param!

  std::ostringstream oss;
  const char *str;

  for (int i = 0; i < argc; ++i) {
      if (i != 0)
        oss << ' ';
      str = JS_ToCString(ctx, argv[i]);
      if (!str)
        return JS_EXCEPTION;
      oss << str;
      JS_FreeCString(ctx, str);
  }
  oss << '\n';

  // Call Java consoleLogHelper(str)
  JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
  assert(jsBridgeContext != nullptr);

  JniContext *jniContext = jsBridgeContext->getJniContext();
  const JniCache *jniCache = jsBridgeContext->getJniCache();

  jniCache->getJsBridgeInterface().consoleLogHelper(JStringLocalRef(jniContext, logType), JStringLocalRef(jniContext, str));

  return JS_UNDEFINED;
}

void quickjs_console_init(JSContext *ctx) {
  JSValue global_obj = JS_GetGlobalObject(ctx);

  JSValue console = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, console, "log", JS_NewCFunction(ctx, js_print, "log", 1));
  JS_SetPropertyStr(ctx, global_obj, "console", console);

  JS_SetPropertyStr(ctx, global_obj, "print",
                    JS_NewCFunction(ctx, js_print, "print", 1));

  JS_FreeValue(ctx, global_obj);
  }
