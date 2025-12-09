//****************************************************************************************************************************************************
//* Zero-Clause BSD (0BSD)
//*
//* Copyright (c) 2025, Mana Battery
//*
//* Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.
//*
//* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
//* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
//* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
//* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//****************************************************************************************************************************************************

#include <Test2/Framework/Lifecycle/DispatchContext.hpp>
#include <Test2/Framework/Lifecycle/ExecutorContext.hpp>
#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace Test2
{
  // ============================================================================
  // Test Fixtures and Helper Classes
  // ============================================================================

  /// @brief Simple test class for source object
  class SourceObject
  {
  public:
    int SourceId{0};
    explicit SourceObject(int id)
      : SourceId(id)
    {
    }
  };

  /// @brief Simple test class for target object
  class TargetObject
  {
  public:
    int TargetId{0};
    explicit TargetObject(int id)
      : TargetId(id)
    {
    }
  };

  /// @brief Test fixture for DispatchContext tests
  class DispatchContextTest : public ::testing::Test
  {
  protected:
    boost::asio::io_context m_sourceIoContext;
    boost::asio::io_context m_targetIoContext;

    void SetUp() override
    {
      // Fresh io_contexts for each test
    }

    void TearDown() override
    {
      // Cleanup if needed
    }
  };

  // ============================================================================
  // Constructor Tests
  // ============================================================================

  TEST_F(DispatchContextTest, Constructor_WithValidContexts_StoresBothContexts)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);

    // Act
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Assert
    EXPECT_TRUE(dispatchContext.IsSourceAlive());
    EXPECT_TRUE(dispatchContext.IsTargetAlive());
  }

  TEST_F(DispatchContextTest, Constructor_MovesContexts_NotCopies)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);

    // Act - Should move contexts
    DispatchContext<SourceObject, TargetObject> dispatchContext(std::move(sourceContext), std::move(targetContext));

    // Assert
    EXPECT_TRUE(dispatchContext.IsSourceAlive());
    EXPECT_TRUE(dispatchContext.IsTargetAlive());
  }

  // ============================================================================
  // GetSourceContext Tests
  // ============================================================================

  TEST_F(DispatchContextTest, GetSourceContext_ReturnsCorrectContext)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(100);
    auto targetPtr = std::make_shared<TargetObject>(200);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act
    const auto& retrievedContext = dispatchContext.GetSourceContext();
    auto locked = retrievedContext.TryLock();

    // Assert
    ASSERT_NE(locked, nullptr);
    EXPECT_EQ(locked->SourceId, 100);
  }

  TEST_F(DispatchContextTest, GetSourceContext_IsConstMethod)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    const DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert - Should compile as const method
    const auto& retrievedContext = dispatchContext.GetSourceContext();
    EXPECT_TRUE(retrievedContext.IsAlive());
  }

  // ============================================================================
  // GetTargetContext Tests
  // ============================================================================

  TEST_F(DispatchContextTest, GetTargetContext_ReturnsCorrectContext)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(100);
    auto targetPtr = std::make_shared<TargetObject>(200);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act
    const auto& retrievedContext = dispatchContext.GetTargetContext();
    auto locked = retrievedContext.TryLock();

    // Assert
    ASSERT_NE(locked, nullptr);
    EXPECT_EQ(locked->TargetId, 200);
  }

  TEST_F(DispatchContextTest, GetTargetContext_IsConstMethod)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    const DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert - Should compile as const method
    const auto& retrievedContext = dispatchContext.GetTargetContext();
    EXPECT_TRUE(retrievedContext.IsAlive());
  }

  // ============================================================================
  // GetSourceExecutor Tests
  // ============================================================================

  TEST_F(DispatchContextTest, GetSourceExecutor_ReturnsExecutor)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act
    auto retrievedExecutor = dispatchContext.GetSourceExecutor();

    // Assert - Executor successfully retrieved
    (void)retrievedExecutor;
    SUCCEED();
  }

  TEST_F(DispatchContextTest, GetSourceExecutor_IsConstMethod)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    const DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert - Should compile as const method
    auto retrievedExecutor = dispatchContext.GetSourceExecutor();
    (void)retrievedExecutor;
    SUCCEED();
  }

  // ============================================================================
  // GetTargetExecutor Tests
  // ============================================================================

  TEST_F(DispatchContextTest, GetTargetExecutor_ReturnsExecutor)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act
    auto retrievedExecutor = dispatchContext.GetTargetExecutor();

    // Assert - Executor successfully retrieved
    (void)retrievedExecutor;
    SUCCEED();
  }

  TEST_F(DispatchContextTest, GetTargetExecutor_IsConstMethod)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    const DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert - Should compile as const method
    auto retrievedExecutor = dispatchContext.GetTargetExecutor();
    (void)retrievedExecutor;
    SUCCEED();
  }

  // ============================================================================
  // IsSourceAlive Tests
  // ============================================================================

  TEST_F(DispatchContextTest, IsSourceAlive_WithAliveSource_ReturnsTrue)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert
    EXPECT_TRUE(dispatchContext.IsSourceAlive());
  }

  TEST_F(DispatchContextTest, IsSourceAlive_WithExpiredSource_ReturnsFalse)
  {
    // Arrange
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(std::make_shared<SourceObject>(1), sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert - Source was immediately destroyed
    EXPECT_FALSE(dispatchContext.IsSourceAlive());
  }

  TEST_F(DispatchContextTest, IsSourceAlive_AfterSourceDestroyed_ReturnsFalse)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act - Destroy source
    sourcePtr.reset();

    // Assert
    EXPECT_FALSE(dispatchContext.IsSourceAlive());
    EXPECT_TRUE(dispatchContext.IsTargetAlive());    // Target should still be alive
  }

  TEST_F(DispatchContextTest, IsSourceAlive_IsConstMethod)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    const DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert - Should compile as const method
    EXPECT_TRUE(dispatchContext.IsSourceAlive());
  }

  TEST_F(DispatchContextTest, IsSourceAlive_IsNoexcept)
  {
    // Assert - Verify IsSourceAlive is declared noexcept
    EXPECT_TRUE(noexcept(std::declval<const DispatchContext<SourceObject, TargetObject>>().IsSourceAlive()));
  }

  // ============================================================================
  // IsTargetAlive Tests
  // ============================================================================

  TEST_F(DispatchContextTest, IsTargetAlive_WithAliveTarget_ReturnsTrue)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert
    EXPECT_TRUE(dispatchContext.IsTargetAlive());
  }

  TEST_F(DispatchContextTest, IsTargetAlive_WithExpiredTarget_ReturnsFalse)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(std::make_shared<TargetObject>(2), targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert - Target was immediately destroyed
    EXPECT_FALSE(dispatchContext.IsTargetAlive());
  }

  TEST_F(DispatchContextTest, IsTargetAlive_AfterTargetDestroyed_ReturnsFalse)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act - Destroy target
    targetPtr.reset();

    // Assert
    EXPECT_TRUE(dispatchContext.IsSourceAlive());    // Source should still be alive
    EXPECT_FALSE(dispatchContext.IsTargetAlive());
  }

  TEST_F(DispatchContextTest, IsTargetAlive_IsConstMethod)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    const DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert - Should compile as const method
    EXPECT_TRUE(dispatchContext.IsTargetAlive());
  }

  TEST_F(DispatchContextTest, IsTargetAlive_IsNoexcept)
  {
    // Assert - Verify IsTargetAlive is declared noexcept
    EXPECT_TRUE(noexcept(std::declval<const DispatchContext<SourceObject, TargetObject>>().IsTargetAlive()));
  }

  // ============================================================================
  // Lifetime Tracking Tests
  // ============================================================================

  TEST_F(DispatchContextTest, LifetimeTracking_IndependentLifetimes)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act - Destroy source, keep target
    sourcePtr.reset();

    // Assert - Source dead, target alive
    EXPECT_FALSE(dispatchContext.IsSourceAlive());
    EXPECT_TRUE(dispatchContext.IsTargetAlive());

    // Act - Destroy target
    targetPtr.reset();

    // Assert - Both dead
    EXPECT_FALSE(dispatchContext.IsSourceAlive());
    EXPECT_FALSE(dispatchContext.IsTargetAlive());
  }

  TEST_F(DispatchContextTest, LifetimeTracking_BothDestroyed)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act - Destroy both
    sourcePtr.reset();
    targetPtr.reset();

    // Assert
    EXPECT_FALSE(dispatchContext.IsSourceAlive());
    EXPECT_FALSE(dispatchContext.IsTargetAlive());
  }

  // ============================================================================
  // Executor Distinction Tests
  // ============================================================================

  TEST_F(DispatchContextTest, ExecutorDistinction_BothExecutorsAccessible)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act
    auto retrievedSourceExecutor = dispatchContext.GetSourceExecutor();
    auto retrievedTargetExecutor = dispatchContext.GetTargetExecutor();

    // Assert - Both executors successfully retrieved
    (void)retrievedSourceExecutor;
    (void)retrievedTargetExecutor;
    SUCCEED();
  }

  TEST_F(DispatchContextTest, ExecutorDistinction_SameIOContext)
  {
    // Arrange - Both use the same io_context
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetPtr = std::make_shared<TargetObject>(2);
    auto sharedExecutor = m_sourceIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sharedExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, sharedExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act
    auto retrievedSourceExecutor = dispatchContext.GetSourceExecutor();
    auto retrievedTargetExecutor = dispatchContext.GetTargetExecutor();

    // Assert - Both executors accessible
    (void)retrievedSourceExecutor;
    (void)retrievedTargetExecutor;
    SUCCEED();
  }

  // ============================================================================
  // Different Template Type Tests
  // ============================================================================

  TEST_F(DispatchContextTest, DifferentTypes_StringAndInt_WorksCorrectly)
  {
    // Arrange
    auto sourcePtr = std::make_shared<std::string>("source");
    auto targetPtr = std::make_shared<int>(42);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<std::string> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<int> targetContext(targetPtr, targetExecutor);

    // Act
    DispatchContext<std::string, int> dispatchContext(sourceContext, targetContext);

    // Assert
    EXPECT_TRUE(dispatchContext.IsSourceAlive());
    EXPECT_TRUE(dispatchContext.IsTargetAlive());

    auto sourceCtx = dispatchContext.GetSourceContext();
    auto targetCtx = dispatchContext.GetTargetContext();
    auto lockedSource = sourceCtx.TryLock();
    auto lockedTarget = targetCtx.TryLock();

    ASSERT_NE(lockedSource, nullptr);
    ASSERT_NE(lockedTarget, nullptr);
    EXPECT_EQ(*lockedSource, "source");
    EXPECT_EQ(*lockedTarget, 42);
  }

  TEST_F(DispatchContextTest, DifferentTypes_SameType_WorksCorrectly)
  {
    // Arrange - Both source and target are same type
    auto sourcePtr = std::make_shared<SourceObject>(100);
    auto targetPtr = std::make_shared<SourceObject>(200);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<SourceObject> targetContext(targetPtr, targetExecutor);

    // Act
    DispatchContext<SourceObject, SourceObject> dispatchContext(sourceContext, targetContext);

    // Assert
    EXPECT_TRUE(dispatchContext.IsSourceAlive());
    EXPECT_TRUE(dispatchContext.IsTargetAlive());

    auto sourceCtx = dispatchContext.GetSourceContext();
    auto targetCtx = dispatchContext.GetTargetContext();
    auto lockedSource = sourceCtx.TryLock();
    auto lockedTarget = targetCtx.TryLock();

    ASSERT_NE(lockedSource, nullptr);
    ASSERT_NE(lockedTarget, nullptr);
    EXPECT_EQ(lockedSource->SourceId, 100);
    EXPECT_EQ(lockedTarget->SourceId, 200);
  }

  // ============================================================================
  // Edge Cases
  // ============================================================================

  TEST_F(DispatchContextTest, EdgeCase_BothImmediatelyExpired)
  {
    // Arrange
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(std::make_shared<SourceObject>(1), sourceExecutor);
    ExecutorContext<TargetObject> targetContext(std::make_shared<TargetObject>(2), targetExecutor);

    // Act
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Assert - Both should be expired
    EXPECT_FALSE(dispatchContext.IsSourceAlive());
    EXPECT_FALSE(dispatchContext.IsTargetAlive());
  }

  TEST_F(DispatchContextTest, EdgeCase_SourceAliveThenExpired)
  {
    // Arrange
    auto sourcePtr = std::make_shared<SourceObject>(1);
    auto targetExecutor = m_targetIoContext.get_executor();
    auto sourceExecutor = m_sourceIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(std::make_shared<TargetObject>(2), targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Assert initial state
    EXPECT_TRUE(dispatchContext.IsSourceAlive());
    EXPECT_FALSE(dispatchContext.IsTargetAlive());

    // Act - Destroy source
    sourcePtr.reset();

    // Assert - Both dead
    EXPECT_FALSE(dispatchContext.IsSourceAlive());
    EXPECT_FALSE(dispatchContext.IsTargetAlive());
  }

  // ============================================================================
  // Integration Tests
  // ============================================================================

  TEST_F(DispatchContextTest, Integration_FullRoundTrip)
  {
    // Arrange - Simulate a full round-trip scenario
    auto sourcePtr = std::make_shared<SourceObject>(10);
    auto targetPtr = std::make_shared<TargetObject>(20);
    auto sourceExecutor = m_sourceIoContext.get_executor();
    auto targetExecutor = m_targetIoContext.get_executor();

    ExecutorContext<SourceObject> sourceContext(sourcePtr, sourceExecutor);
    ExecutorContext<TargetObject> targetContext(targetPtr, targetExecutor);
    DispatchContext<SourceObject, TargetObject> dispatchContext(sourceContext, targetContext);

    // Act & Assert - Verify all components work together
    EXPECT_TRUE(dispatchContext.IsSourceAlive());
    EXPECT_TRUE(dispatchContext.IsTargetAlive());

    auto srcExec = dispatchContext.GetSourceExecutor();
    auto tgtExec = dispatchContext.GetTargetExecutor();
    (void)srcExec;
    (void)tgtExec;

    const auto& srcCtx = dispatchContext.GetSourceContext();
    const auto& tgtCtx = dispatchContext.GetTargetContext();
    auto lockedSrc = srcCtx.TryLock();
    auto lockedTgt = tgtCtx.TryLock();

    ASSERT_NE(lockedSrc, nullptr);
    ASSERT_NE(lockedTgt, nullptr);
    EXPECT_EQ(lockedSrc->SourceId, 10);
    EXPECT_EQ(lockedTgt->TargetId, 20);
  }

}    // namespace Test2
