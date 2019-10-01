#include "JniHelpersTestFixture.h"

#include "jnimock/jnimock.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "JniContext.h"
#include "JStringLocalRef.h"

#include <iostream>

using testing::_;
using testing::Return;
using namespace jnimock;

class JStringLocalRefTest: public JniHelpersTestFixture {};

TEST_F(JStringLocalRefTest, NullStringRefs) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  JStringLocalRef stringRef1;
  EXPECT_EQ(nullptr, stringRef1.get());
  EXPECT_EQ(nullptr, stringRef1.m_sharedAutoRelease.get());

  JStringLocalRef stringRef2(stringRef1);
  EXPECT_EQ(nullptr, stringRef2.get());
  EXPECT_EQ(nullptr, stringRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(nullptr, stringRef2.m_sharedAutoRelease.get());

  JStringLocalRef stringRef3(std::move(stringRef1));
  EXPECT_EQ(nullptr, stringRef3.get());
  EXPECT_EQ(nullptr, stringRef3.m_sharedAutoRelease.get());
}

TEST_F(JStringLocalRefTest, FromJavaString) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  JStringLocalRef stringRef1(jniContext, jstring(543));
  EXPECT_EQ(jstring(543), stringRef1.get());
  EXPECT_NE(nullptr, stringRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(1, stringRef1.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, GetStringUTFChars(_, _))
    .Times(1)
    .WillOnce(Return("firstString"));

  // To UTF-8 string
  const char *utf8 = stringRef1.toUtf8Chars();

  EXPECT_EQ("firstString", std::string(utf8));

  EXPECT_CALL(*env, ReleaseStringUTFChars(_, _))
    .Times(1);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(1);
}

