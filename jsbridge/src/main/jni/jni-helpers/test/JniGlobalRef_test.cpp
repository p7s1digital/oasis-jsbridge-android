#include "JniHelpersTestFixture.h"

#include "jnimock/jnimock.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "JniContext.h"
#include "JniGlobalRef.h"

#include <iostream>

using testing::_;
using testing::Return;
using namespace jnimock;

class JniGlobalRefTest: public JniHelpersTestFixture {};

TEST_F(JniGlobalRefTest, NullGlobalRefs) {

  EXPECT_CALL(*env, NewGlobalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);

  JniGlobalRef<jobject> globalRef1;
  EXPECT_EQ(nullptr, globalRef1.get());
  EXPECT_EQ(nullptr, globalRef1.m_sharedAutoRelease.get());

  JniGlobalRef<jobject> globalRef2(globalRef1);
  EXPECT_EQ(nullptr, globalRef2.get());
  EXPECT_EQ(nullptr, globalRef2.m_sharedAutoRelease.get());

  JniGlobalRef<jobject> globalRef3(std::move(globalRef1));
  EXPECT_EQ(nullptr, globalRef3.get());
  EXPECT_EQ(nullptr, globalRef3.m_sharedAutoRelease.get());

  JniLocalRef<jobject> localRef;
  JniGlobalRef<jobject> globalRef4(localRef);
  EXPECT_EQ(nullptr, globalRef4.get());
  EXPECT_EQ(nullptr, globalRef4.m_sharedAutoRelease.get());
}

TEST_F(JniGlobalRefTest, CopyConstructor) {

  EXPECT_CALL(*env, NewGlobalRef(jobject(123)))
    .Times(1)
    .WillOnce(Return(jobject(123000)));
  EXPECT_CALL(*env, NewGlobalRef(jobject(456)))
    .Times(1)
    .WillOnce(Return(jobject(456000)));

  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);

  // Local ref
  JniLocalRef<jobject> localRef1(jniContext, jobject(123));
  JniLocalRef<jobject> localRef2(jniContext, jobject(456));

  // Global ref 1
  JniGlobalRef<jobject> globalRef1(localRef1);
  EXPECT_EQ(jobject(123000), globalRef1.get());
  EXPECT_EQ(1, globalRef1.m_sharedAutoRelease.use_count());

  // Global ref 1 (copy1)
  JniGlobalRef<jobject> globalRef1_copy1(globalRef1);
  EXPECT_EQ(jobject(123000), globalRef1_copy1.get());
  EXPECT_EQ(globalRef1.m_sharedAutoRelease, globalRef1_copy1.m_sharedAutoRelease);
  EXPECT_EQ(2, globalRef1.m_sharedAutoRelease.use_count());

  // Global ref 1 (copy2)
  JniGlobalRef<jobject> globalRef1_copy2(globalRef1);
  EXPECT_EQ(jobject(123000), globalRef1_copy2.get());
  EXPECT_EQ(globalRef1.m_sharedAutoRelease, globalRef1_copy2.m_sharedAutoRelease);
  EXPECT_EQ(3, globalRef1.m_sharedAutoRelease.use_count());

  // Global ref 2
  JniGlobalRef<jobject> globalRef2(localRef2);
  EXPECT_EQ(jobject(456000), globalRef2.get());

  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);
  EXPECT_CALL(*env, DeleteLocalRef(jobject(456)))
    .Times(1);
  EXPECT_CALL(*env, DeleteGlobalRef(jobject(123000)))
    .Times(1);
  EXPECT_CALL(*env, DeleteGlobalRef(jobject(456000)))
    .Times(1);
}

TEST_F(JniGlobalRefTest, MoveConstructor) {

  EXPECT_CALL(*env, NewGlobalRef(jobject(123)))
    .Times(1)
    .WillOnce(Return(jobject(123000)));

  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);

  // Original local + global ref
  JniLocalRef<jobject> localRef(jniContext, jobject(123));
  JniGlobalRef<jobject> globalRef1(localRef);

  // Release the source local ref
  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);
  localRef.release();

  // 1st copy
  auto globalRef2 = JniGlobalRef<jobject>(globalRef1);
  EXPECT_NE(nullptr, globalRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(2, globalRef1.m_sharedAutoRelease.use_count());
  EXPECT_EQ(globalRef1.m_sharedAutoRelease, globalRef2.m_sharedAutoRelease);

  // Move original ref
  auto globalRef3 = std::move(globalRef1);
  EXPECT_EQ(jobject(123000), globalRef3.get());
  EXPECT_NE(nullptr, globalRef3.m_sharedAutoRelease.get());
  EXPECT_EQ(globalRef2.m_sharedAutoRelease, globalRef3.m_sharedAutoRelease);
  EXPECT_EQ(2, globalRef2.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteGlobalRef(jobject(123000)))
    .Times(1);
}

TEST_F(JniGlobalRefTest, AssignmentOperator) {

  EXPECT_CALL(*env, NewGlobalRef(jobject(123)))
    .Times(1)
    .WillOnce(Return(jobject(123000)));

  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);

  // Original local + global ref
  JniLocalRef<jobject> localRef(jniContext, jobject(123));
  JniGlobalRef<jobject> globalRef1(localRef);

  // Release the source local ref
  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);
  localRef.release();

  // 1st assignement
  auto globalRef2 = globalRef1;

  EXPECT_EQ(jobject(123000), globalRef2.get());
  EXPECT_EQ(globalRef1.m_sharedAutoRelease, globalRef2.m_sharedAutoRelease);
  EXPECT_EQ(2, globalRef1.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, NewGlobalRef(jobject(456)))
    .Times(1)
    .WillOnce(Return(jobject(456000)));
  EXPECT_CALL(*env, DeleteGlobalRef(jobject(456000)))
    .Times(1);
  EXPECT_CALL(*env, DeleteLocalRef(jobject(456)))
    .Times(1);

  // 2nd assignment (from another existing global ref)
  auto globalRef3 = JniGlobalRef<jobject>(JniLocalRef<jobject>(jniContext, jobject(456)));
  globalRef3 = globalRef1;

  // Original jobject of globalRef3 should have been deleted on above move assignment
  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);

  EXPECT_EQ(jobject(123000), globalRef3.get());
  EXPECT_EQ(globalRef1.m_sharedAutoRelease, globalRef3.m_sharedAutoRelease);
  EXPECT_EQ(3, globalRef1.m_sharedAutoRelease.use_count());

  // Release original (globalRef1)
  globalRef1.release();
  EXPECT_EQ(2, globalRef2.m_sharedAutoRelease.use_count());

  // Release 2nd copy (globalRef3)
  globalRef3.release();
  EXPECT_EQ(1, globalRef2.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteGlobalRef(jobject(123000)))
    .Times(1);

  // Remaining one (globalRef2) will be auto-deleted
}

TEST_F(JniGlobalRefTest, MoveAssignmentOperator) {

  EXPECT_CALL(*env, NewGlobalRef(jobject(123)))
    .Times(1)
    .WillOnce(Return(jobject(123000)));

  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);

  // Original local + global ref
  JniLocalRef<jobject> localRef(jniContext, jobject(123));
  JniGlobalRef<jobject> globalRef1(localRef);

  // Release the source local ref
  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);
  localRef.release();

  EXPECT_EQ(jobject(123000), globalRef1.get());
  EXPECT_EQ(1, globalRef1.m_sharedAutoRelease.use_count());

  // move globalRef1 -> globalRef2
  auto globalRef2 = std::move(globalRef1);

  EXPECT_EQ(jobject(123000), globalRef2.get());
  EXPECT_EQ(1, globalRef2.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, NewGlobalRef(jobject(456)))
    .Times(1)
    .WillOnce(Return(jobject(456000)));
  EXPECT_CALL(*env, DeleteLocalRef(jobject(456)))
    .Times(1);
  EXPECT_CALL(*env, DeleteGlobalRef(jobject(456000)))
    .Times(1);

  // move globalRef2 -> globalRef3 (existing global ref)
  auto globalRef3 = JniGlobalRef<jobject>(JniLocalRef<jobject>(jniContext, jobject(456)));
  globalRef3 = std::move(globalRef2);

  // Original jobject of globalRef3 should have been deleted on above move assignment
  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);

  EXPECT_EQ(jobject(123000), globalRef3.get());
  EXPECT_EQ(1, globalRef3.m_sharedAutoRelease.use_count());

  // Remaining one (globalRef3) will be auto-deleted
  EXPECT_CALL(*env, DeleteGlobalRef(jobject(123000)))
    .Times(1);
}

TEST_F(JniGlobalRefTest, NoAutoRelease) {

  EXPECT_CALL(*env, NewGlobalRef(jobject(123)))
    .Times(1)
    .WillOnce(Return(jobject(123000)));

  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);

  // Original local + global ref
  JniLocalRef<jobject> localRef(jniContext, jobject(123));
  JniGlobalRef<jobject> globalRef1(localRef, JniRefReleaseMode::Never);

  // Release the source local ref
  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);
  localRef.release();

  EXPECT_EQ(jobject(123000), globalRef1.get());
  EXPECT_EQ(nullptr, globalRef1.m_sharedAutoRelease.get());

  // 1st copy
  JniGlobalRef<jobject> globalRef2(globalRef1);

  EXPECT_EQ(jobject(123000), globalRef2.get());
  EXPECT_EQ(nullptr, globalRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(nullptr, globalRef2.m_sharedAutoRelease.get());

  // 2nd copy
  JniGlobalRef<jobject> globalRef3(globalRef1);

  EXPECT_EQ(jobject(123000), globalRef3.get());
  EXPECT_EQ(nullptr, globalRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(nullptr, globalRef2.m_sharedAutoRelease.get());

  // Manual release of 2 copies
  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);
  globalRef1.release();
  globalRef2.release();

  EXPECT_EQ(nullptr, globalRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(nullptr, globalRef2.m_sharedAutoRelease.get());
  EXPECT_EQ(nullptr, globalRef3.m_sharedAutoRelease.get());

  // Additional manual release are ignore
  globalRef1.release();
  globalRef2.release();
  globalRef3.release();
}

TEST_F(JniGlobalRefTest, Detach) {

  EXPECT_CALL(*env, NewGlobalRef(jobject(123)))
    .Times(1)
    .WillOnce(Return(jobject(123000)));

  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);

  // Original local + global ref
  JniLocalRef<jobject> localRef(jniContext, jobject(123));
  JniGlobalRef<jobject> globalRef1(localRef);

  // Release the source local ref
  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);
  localRef.release();

  EXPECT_EQ(jobject(123000), globalRef1.get());
  EXPECT_EQ(1, globalRef1.m_sharedAutoRelease.use_count());

  // Copy
  JniGlobalRef<jobject> globalRef2(globalRef1);

  EXPECT_EQ(jobject(123000), globalRef2.get());
  EXPECT_EQ(globalRef1.m_sharedAutoRelease, globalRef2.m_sharedAutoRelease);
  EXPECT_EQ(2, globalRef1.m_sharedAutoRelease.use_count());

  // Detach original ref
  globalRef1.detach();
  EXPECT_EQ(nullptr, globalRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(1, globalRef2.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteGlobalRef(_))
    .Times(0);

  // globalRef2 will not be auto-released
}

