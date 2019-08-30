# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.6.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -DANDROID_STL=c++_static -DANDROID_TOOLCHAIN=clang -DDUK_DEBUG_PORT=${DEBUGGER_SERVER_PORT}")

message("CMake - Flavor = ${FLAVOR}")

include_directories(
    src/main/jni
)

add_library(
    ${JNI_LIB_NAME} SHARED

    src/main/jni/custom_stringify.cpp
    src/main/jni/de_prosiebensat1digital_oasisjsbridge_JsBridge.cpp
    src/main/jni/ArgumentLoader.cpp
    src/main/jni/JavaMethod.cpp
    src/main/jni/JavaScriptLambda.cpp
    src/main/jni/JavaScriptMethod.cpp
    src/main/jni/JavaScriptObject.cpp
    src/main/jni/JavaTypeMap.cpp
    src/main/jni/log.cpp
    src/main/jni/java-types/Array.cpp
    src/main/jni/java-types/Boolean.cpp
    src/main/jni/java-types/BoxedPrimitive.cpp
    src/main/jni/java-types/Double.cpp
    src/main/jni/java-types/Float.cpp
    src/main/jni/java-types/Integer.cpp
    src/main/jni/java-types/JsValue.cpp
    src/main/jni/java-types/Long.cpp
    src/main/jni/java-types/Object.cpp
    src/main/jni/java-types/String.cpp
    src/main/jni/java-types/Void.cpp
    src/main/jni/jni-helpers/JniContext.cpp
    src/main/jni/jni-helpers/JniLocalFrame.cpp
    src/main/jni/jni-helpers/JniLocalRefStats.cpp
    src/main/jni/jni-helpers/JniRefHelper.cpp
)

if (FLAVOR STREQUAL "DUKTAPE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDUKTAPE")
    include_directories(src/duktape/jni)

    target_sources(${JNI_LIB_NAME} PUBLIC
        src/main/jni/duk_console.cpp
        src/main/jni/duktape/duk_trans_socket_unix.c
        src/main/jni/duktape/duktape.cpp
        src/main/jni/JavaScriptObjectMapper.cpp
        src/main/jni/JsBridgeContext_duktape.cpp
        src/main/jni/DuktapeUtils.cpp
        src/main/jni/JavaType_duktape.cpp
        src/main/jni/java-types/Deferred_duktape.cpp
        src/main/jni/java-types/FunctionX_duktape.cpp
        src/main/jni/java-types/JsonObjectWrapper_duktape.cpp
    )
elseif (FLAVOR STREQUAL "QUICKJS")
    file (STRINGS "src/main/jni/quickjs/VERSION" QUICKJS_VERSION)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCONFIG_VERSION=\\\"${QUICKJS_VERSION}\\\"")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQUICKJS")
    include_directories(src/quickjs/jni)

    target_sources(${JNI_LIB_NAME} PUBLIC
        src/main/jni/quickjs_console.cpp
        src/main/jni/QuickJsUtils.cpp
        src/main/jni/quickjs/cutils.c
        src/main/jni/quickjs/libregexp.c
        src/main/jni/quickjs/libunicode.c
        src/main/jni/quickjs/quickjs.c
        src/main/jni/JsBridgeContext_quickjs.cpp
        src/main/jni/JavaType_quickjs.cpp
        src/main/jni/java-types/Deferred_quickjs.cpp
        src/main/jni/java-types/FunctionX_quickjs.cpp
        src/main/jni/java-types/JsonObjectWrapper_quickjs.cpp
    )
endif (FLAVOR STREQUAL "DUKTAPE")

find_library(log-lib log)
target_link_libraries(${JNI_LIB_NAME} ${log-lib})
