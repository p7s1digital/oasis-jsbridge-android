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

class JniLocalRefTest: public JniHelpersTestFixture {};

TEST_F(JniLocalRefTest, NullLocalRefs) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  JniLocalRef<jobject> localRef1;
  EXPECT_EQ(nullptr, localRef1.get());
  EXPECT_EQ(nullptr, localRef1.m_sharedAutoRelease.get());

  JniLocalRef<jobject> localRef2(jniContext, nullptr);
  EXPECT_EQ(nullptr, localRef2.get());
  EXPECT_EQ(nullptr, localRef2.m_sharedAutoRelease.get());

  JniLocalRef<jobject> localRef3(localRef1);
  EXPECT_EQ(nullptr, localRef3.get());
  EXPECT_EQ(nullptr, localRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(nullptr, localRef3.m_sharedAutoRelease.get());

  JniLocalRef<jobject> localRef4(std::move(localRef1));
  EXPECT_EQ(nullptr, localRef4.get());
  EXPECT_EQ(nullptr, localRef4.m_sharedAutoRelease.get());
}

TEST_F(JniLocalRefTest, CopyConstructor) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  // Original local ref
  auto pLocalRef1 = new JniLocalRef<jobject>(jniContext, jobject(123));

  EXPECT_EQ(jobject(123), pLocalRef1->get());
  EXPECT_EQ(1, pLocalRef1->m_sharedAutoRelease.use_count());

  // 1st copy
  auto pLocalRef2 = new JniLocalRef<jobject>(*pLocalRef1);

  EXPECT_EQ(jobject(123), pLocalRef2->get());
  EXPECT_EQ(pLocalRef1->m_sharedAutoRelease, pLocalRef2->m_sharedAutoRelease);
  EXPECT_EQ(2, pLocalRef1->m_sharedAutoRelease.use_count());

  // 2nd copy
  auto pLocalRef3 = new JniLocalRef<jobject>(*pLocalRef1);

  EXPECT_EQ(jobject(123), pLocalRef3->get());
  EXPECT_EQ(pLocalRef1->m_sharedAutoRelease, pLocalRef3->m_sharedAutoRelease);
  EXPECT_EQ(3, pLocalRef1->m_sharedAutoRelease.use_count());

  // Delete original (pLocalRef1)
  delete pLocalRef1;
  EXPECT_EQ(2, pLocalRef2->m_sharedAutoRelease.use_count());

  // Delete 2nd copy (pLocalRef3)
  delete pLocalRef3;
  EXPECT_EQ(1, pLocalRef2->m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);

  // Delete remaining one
  delete pLocalRef2;
}

TEST_F(JniLocalRefTest, MoveConstructor) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  // Original local ref
  JniLocalRef<jobject> localRef1(jniContext, jobject(123));

  // 1st copy
  JniLocalRef<jobject> localRef2(localRef1);
  EXPECT_NE(nullptr, localRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(2, localRef1.m_sharedAutoRelease.use_count());
  EXPECT_EQ(localRef1.m_sharedAutoRelease, localRef2.m_sharedAutoRelease);

  // Move original ref
  auto localRef3 = std::move(localRef1);
  EXPECT_EQ(jobject(123), localRef3.get());
  EXPECT_NE(nullptr, localRef3.m_sharedAutoRelease.get());
  EXPECT_EQ(localRef2.m_sharedAutoRelease, localRef3.m_sharedAutoRelease);
  EXPECT_EQ(2, localRef2.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);
}

TEST_F(JniLocalRefTest, ConstructFromGlobalRef) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, NewGlobalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  // From null global ref
  JniGlobalRef<jobject> globalRef1;
  JniLocalRef<jobject> localRef1(globalRef1);
  EXPECT_EQ(nullptr, localRef1.get());

  EXPECT_CALL(*env, NewGlobalRef(jobject(123)))
    .Times(1)
    .WillOnce(Return(jobject(123000)));

  // Create non-null global ref
  JniLocalRef<jobject> localRefSource2(jniContext, jobject(123));
  JniGlobalRef<jobject> globalRef2(localRefSource2);

  EXPECT_CALL(*env, NewLocalRef(jobject(123000)))
    .Times(1)
    .WillOnce(Return(jobject(123123)));

  // Create local ref from global ref
  JniLocalRef<jobject> localRef2(globalRef2);
  EXPECT_EQ(jobject(123123), localRef2.get());

  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);
  EXPECT_CALL(*env, DeleteGlobalRef(jobject(123000)))
    .Times(1);
  EXPECT_CALL(*env, DeleteLocalRef(jobject(123123)))
    .Times(1);
}

TEST_F(JniLocalRefTest, AssignmentOperator) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  // Original local ref
  JniLocalRef<jobject> localRef1(jniContext, jobject(123));

  EXPECT_EQ(jobject(123), localRef1.get());
  EXPECT_EQ(1, localRef1.m_sharedAutoRelease.use_count());

  // 1st assignment
  auto localRef2 = localRef1;

  EXPECT_EQ(jobject(123), localRef2.get());
  EXPECT_EQ(localRef1.m_sharedAutoRelease, localRef2.m_sharedAutoRelease);
  EXPECT_EQ(2, localRef1.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteLocalRef(jobject(456)))
    .Times(1);

  // 2nd assignment (from another existing local ref)
  JniLocalRef<jobject> localRef3(jniContext, jobject(456));
  localRef3 = localRef1;

  // Original jobject of localRef3 should have been deleted on above move assignment
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  EXPECT_EQ(jobject(123), localRef3.get());
  EXPECT_EQ(localRef1.m_sharedAutoRelease, localRef3.m_sharedAutoRelease);
  EXPECT_EQ(3, localRef1.m_sharedAutoRelease.use_count());

  // Release original (localRef1)
  localRef1.release();
  EXPECT_EQ(2, localRef2.m_sharedAutoRelease.use_count());

  // Release 2nd copy (localRef3)
  localRef3.release();
  EXPECT_EQ(1, localRef2.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);

  // Remaining one (localRef2) will be auto-deleted
}

TEST_F(JniLocalRefTest, MoveAssignmentOperator) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  // Original local ref
  JniLocalRef<jobject> localRef1(jniContext, jobject(123));

  EXPECT_EQ(jobject(123), localRef1.get());
  EXPECT_EQ(1, localRef1.m_sharedAutoRelease.use_count());

  // move localRef1 -> localRef2
  auto localRef2 = std::move(localRef1);

  EXPECT_EQ(jobject(123), localRef2.get());
  EXPECT_EQ(1, localRef2.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteLocalRef(jobject(456)))
    .Times(1);

  // move localRef2 -> localRef3 (existing local ref)
  JniLocalRef<jobject> localRef3(jniContext, jobject(456));
  localRef3 = std::move(localRef2);

  // Original jobject of localRef3 should have been deleted on above move assignment
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  EXPECT_EQ(jobject(123), localRef3.get());
  EXPECT_EQ(1, localRef3.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);

  // Remaining one (localRef3) will be auto-deleted
}

TEST_F(JniLocalRefTest, NoAutoRelease) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  // Original local ref
  JniLocalRef<jobject> localRef1(jniContext, jobject(123), JniRefReleaseMode::Never);

  EXPECT_EQ(jobject(123), localRef1.get());
  EXPECT_EQ(nullptr, localRef1.m_sharedAutoRelease.get());

  // 1st copy
  JniLocalRef<jobject> localRef2(localRef1);

  EXPECT_EQ(jobject(123), localRef2.get());
  EXPECT_EQ(nullptr, localRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(nullptr, localRef2.m_sharedAutoRelease.get());

  // 2nd copy
  JniLocalRef<jobject> localRef3(localRef1);

  EXPECT_EQ(jobject(123), localRef3.get());
  EXPECT_EQ(nullptr, localRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(nullptr, localRef2.m_sharedAutoRelease.get());

  // Manual release of 2 copies
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);
  localRef1.release();
  localRef2.release();

  EXPECT_EQ(nullptr, localRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(nullptr, localRef2.m_sharedAutoRelease.get());
  EXPECT_EQ(nullptr, localRef3.m_sharedAutoRelease.get());

  // Additional manual release are ignore
  localRef1.release();
  localRef2.release();
  localRef3.release();
}

TEST_F(JniLocalRefTest, Detach) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  // Original local ref
  JniLocalRef<jobject> localRef1(jniContext, jobject(123));

  EXPECT_EQ(jobject(123), localRef1.get());
  EXPECT_EQ(1, localRef1.m_sharedAutoRelease.use_count());

  // Copy
  JniLocalRef<jobject> localRef2(localRef1);

  EXPECT_EQ(jobject(123), localRef2.get());
  EXPECT_EQ(localRef1.m_sharedAutoRelease, localRef2.m_sharedAutoRelease);
  EXPECT_EQ(2, localRef1.m_sharedAutoRelease.use_count());

  // Detach original ref
  localRef1.detach();
  EXPECT_EQ(nullptr, localRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(1, localRef2.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  // localRef2 will not be auto-released
}

TEST_F(JniLocalRefTest, Reset) {

  EXPECT_CALL(*env, NewLocalRef(_))
    .Times(0);
  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);

  // Original local ref
  JniLocalRef<jobject> localRef1(jniContext, jobject(123));

  EXPECT_EQ(jobject(123), localRef1.get());
  EXPECT_EQ(1, localRef1.m_sharedAutoRelease.use_count());

  // Copy
  JniLocalRef<jobject> localRef2(localRef1);

  EXPECT_EQ(jobject(123), localRef2.get());
  EXPECT_EQ(localRef1.m_sharedAutoRelease, localRef2.m_sharedAutoRelease);
  EXPECT_EQ(2, localRef1.m_sharedAutoRelease.use_count());

  // Reset original ref
  localRef1.reset();
  EXPECT_EQ(nullptr, localRef1.m_sharedAutoRelease.get());
  EXPECT_EQ(1, localRef2.m_sharedAutoRelease.use_count());

  EXPECT_CALL(*env, DeleteLocalRef(jobject(123)))
    .Times(1);

  // Reset remaining ref (localRef2)
  localRef2.reset();
  EXPECT_EQ(nullptr, localRef2.m_sharedAutoRelease.get());

  EXPECT_CALL(*env, DeleteLocalRef(_))
    .Times(0);
}

