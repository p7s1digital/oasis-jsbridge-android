#include "jnimock/jnimock.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "JniContext.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;
using namespace jnimock;

class JniHelpersTestFixture : public testing::Test {

protected:
  JniHelpersTestFixture() = default;
   
  void SetUp( ) {
    jvm = createJavaVMMock();
    env = createJNIEnvMock();

    EXPECT_CALL(*env, GetJavaVM(_))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<0>(jvm), Return(jint(0))));

    EXPECT_CALL(*jvm, AttachCurrentThread(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<0>(env), Return(jint(0))));

    jniContext = new JniContext(env);
  }

  void TearDown() {
    destroyJNIEnvMock(env);
    destroyJavaVMMock(jvm);
  }

  JavaVMMock * jvm;
  JNIEnvMock * env;
  const JniContext * jniContext = nullptr;
};

