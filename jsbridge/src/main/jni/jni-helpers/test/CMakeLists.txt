cmake_minimum_required(VERSION 3.6.0)

project(JniHelpersUnitTest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fprofile-instr-generate -fcoverage-mapping")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-instr-generate")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG_JNI_LOCALREFS")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG_JNI_GLOBALREFS")

# GoogleMock
enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIR})

include_directories(
    ..
    mocks
)

include_directories(SYSTEM
    .
    jni
)

add_executable(runUnitTests
  JniLocalRef_test.cpp
  JniGlobalRef_test.cpp
  JStringLocalRef_test.cpp

  ../JniContext.cpp
  ../JniLocalFrame.cpp
  ../JniRefStats.cpp
  ../JniRefHelper.cpp

  jnimock/jnimock.cpp
)

target_link_libraries(runUnitTests ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY} /usr/local/lib/libgmock.a /usr/local/lib/libgmock_main.a)

add_test(
  runUnitTests
  runUnitTests
)

