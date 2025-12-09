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

  /// @brief Simple test class for lifetime tracking
  class TestObject
  {
  public:
    int Value{0};
    explicit TestObject(int value)
      : Value(value)
    {
    }
  };

  /// @brief Test fixture for ExecutorContext tests
  class ExecutorContextTest : public ::testing::Test
  {
  protected:
    boost::asio::io_context m_ioContext;

    void SetUp() override
    {
      // Fresh io_context for each test
    }

    void TearDown() override
    {
      // Cleanup if needed
    }
  };

  // ============================================================================
  // Constructor Tests
  // ============================================================================

  TEST_F(ExecutorContextTest, Constructor_WithValidSharedPtr_StoresExecutorAndWeakPtr)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(42);
    auto executor = m_ioContext.get_executor();

    // Act
    ExecutorContext<TestObject> context(sharedPtr, executor);

    // Assert
    EXPECT_TRUE(context.IsAlive());
    auto locked = context.TryLock();
    ASSERT_NE(locked, nullptr);
    EXPECT_EQ(locked->Value, 42);
  }

  TEST_F(ExecutorContextTest, Constructor_StoresExecutor)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(100);
    auto executor = m_ioContext.get_executor();

    // Act
    ExecutorContext<TestObject> context(sharedPtr, executor);

    // Assert - Context should store the executor (verified by successful retrieval)
    const auto& retrievedExecutor = context.GetExecutor();
    (void)retrievedExecutor;    // Executor successfully retrieved
  }

  // ============================================================================
  // GetExecutor Tests
  // ============================================================================

  TEST_F(ExecutorContextTest, GetExecutor_ReturnsExecutor)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(1);
    auto executor = m_ioContext.get_executor();
    ExecutorContext<TestObject> context(sharedPtr, executor);

    // Act
    const auto& retrievedExecutor = context.GetExecutor();

    // Assert - Executor successfully retrieved
    (void)retrievedExecutor;
    SUCCEED();
  }

  TEST_F(ExecutorContextTest, GetExecutor_IsConstMethod)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(1);
    auto executor = m_ioContext.get_executor();
    const ExecutorContext<TestObject> context(sharedPtr, executor);

    // Act & Assert - Should compile as const method
    const auto& retrievedExecutor = context.GetExecutor();
    (void)retrievedExecutor;
    SUCCEED();
  }

  // ============================================================================
  // GetWeakPtr Tests
  // ============================================================================

  TEST_F(ExecutorContextTest, GetWeakPtr_ReturnsWeakPtrToObject)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(42);
    auto executor = m_ioContext.get_executor();
    ExecutorContext<TestObject> context(sharedPtr, executor);

    // Act
    const auto& weakPtr = context.GetWeakPtr();
    auto locked = weakPtr.lock();

    // Assert
    ASSERT_NE(locked, nullptr);
    EXPECT_EQ(locked->Value, 42);
  }

  TEST_F(ExecutorContextTest, GetWeakPtr_IsConstMethod)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(1);
    auto executor = m_ioContext.get_executor();
    const ExecutorContext<TestObject> context(sharedPtr, executor);

    // Act & Assert - Should compile as const method
    const auto& weakPtr = context.GetWeakPtr();
    EXPECT_FALSE(weakPtr.expired());
  }

  // ============================================================================
  // TryLock Tests
  // ============================================================================

  TEST_F(ExecutorContextTest, TryLock_WithAliveObject_ReturnsValidSharedPtr)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(123);
    auto executor = m_ioContext.get_executor();
    ExecutorContext<TestObject> context(sharedPtr, executor);

    // Act
    auto locked = context.TryLock();

    // Assert
    ASSERT_NE(locked, nullptr);
    EXPECT_EQ(locked->Value, 123);
  }

  TEST_F(ExecutorContextTest, TryLock_WithExpiredObject_ReturnsNullptr)
  {
    // Arrange
    auto executor = m_ioContext.get_executor();
    ExecutorContext<TestObject> context(std::make_shared<TestObject>(42), executor);
    // Let the original shared_ptr go out of scope

    // Act
    auto locked = context.TryLock();

    // Assert
    EXPECT_EQ(locked, nullptr);
  }

  TEST_F(ExecutorContextTest, TryLock_AfterObjectDestroyed_ReturnsNullptr)
  {
    // Arrange
    auto executor = m_ioContext.get_executor();
    std::shared_ptr<TestObject> sharedPtr = std::make_shared<TestObject>(999);
    ExecutorContext<TestObject> context(sharedPtr, executor);

    // Act - Destroy the shared object
    sharedPtr.reset();
    auto locked = context.TryLock();

    // Assert
    EXPECT_EQ(locked, nullptr);
  }

  TEST_F(ExecutorContextTest, TryLock_IsConstMethod)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(1);
    auto executor = m_ioContext.get_executor();
    const ExecutorContext<TestObject> context(sharedPtr, executor);

    // Act & Assert - Should compile as const method
    auto locked = context.TryLock();
    EXPECT_NE(locked, nullptr);
  }

  TEST_F(ExecutorContextTest, TryLock_IsNoexcept)
  {
    // Assert - Verify TryLock is declared noexcept
    EXPECT_TRUE(noexcept(std::declval<const ExecutorContext<TestObject>>().TryLock()));
  }

  // ============================================================================
  // IsAlive Tests
  // ============================================================================

  TEST_F(ExecutorContextTest, IsAlive_WithAliveObject_ReturnsTrue)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(42);
    auto executor = m_ioContext.get_executor();
    ExecutorContext<TestObject> context(sharedPtr, executor);

    // Act & Assert
    EXPECT_TRUE(context.IsAlive());
  }

  TEST_F(ExecutorContextTest, IsAlive_WithExpiredObject_ReturnsFalse)
  {
    // Arrange
    auto executor = m_ioContext.get_executor();
    ExecutorContext<TestObject> context(std::make_shared<TestObject>(42), executor);
    // Original shared_ptr goes out of scope

    // Act & Assert
    EXPECT_FALSE(context.IsAlive());
  }

  TEST_F(ExecutorContextTest, IsAlive_AfterObjectDestroyed_ReturnsFalse)
  {
    // Arrange
    auto executor = m_ioContext.get_executor();
    std::shared_ptr<TestObject> sharedPtr = std::make_shared<TestObject>(999);
    ExecutorContext<TestObject> context(sharedPtr, executor);

    // Act - Destroy the shared object
    sharedPtr.reset();

    // Assert
    EXPECT_FALSE(context.IsAlive());
  }

  TEST_F(ExecutorContextTest, IsAlive_IsConstMethod)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(1);
    auto executor = m_ioContext.get_executor();
    const ExecutorContext<TestObject> context(sharedPtr, executor);

    // Act & Assert - Should compile as const method
    EXPECT_TRUE(context.IsAlive());
  }

  TEST_F(ExecutorContextTest, IsAlive_IsNoexcept)
  {
    // Assert - Verify IsAlive is declared noexcept
    EXPECT_TRUE(noexcept(std::declval<const ExecutorContext<TestObject>>().IsAlive()));
  }

  // ============================================================================
  // Lifetime Tracking Tests
  // ============================================================================

  TEST_F(ExecutorContextTest, LifetimeTracking_MultipleReferences_ObjectStaysAlive)
  {
    // Arrange
    auto sharedPtr1 = std::make_shared<TestObject>(42);
    auto sharedPtr2 = sharedPtr1;    // Second reference
    auto executor = m_ioContext.get_executor();
    ExecutorContext<TestObject> context(sharedPtr1, executor);

    // Act - Release first reference
    sharedPtr1.reset();

    // Assert - Object should still be alive due to sharedPtr2
    EXPECT_TRUE(context.IsAlive());
    auto locked = context.TryLock();
    ASSERT_NE(locked, nullptr);
    EXPECT_EQ(locked->Value, 42);
  }

  TEST_F(ExecutorContextTest, LifetimeTracking_AllReferencesReleased_ObjectDies)
  {
    // Arrange
    auto sharedPtr1 = std::make_shared<TestObject>(42);
    auto sharedPtr2 = sharedPtr1;
    auto executor = m_ioContext.get_executor();
    ExecutorContext<TestObject> context(sharedPtr1, executor);

    // Act - Release all references
    sharedPtr1.reset();
    sharedPtr2.reset();

    // Assert
    EXPECT_FALSE(context.IsAlive());
    EXPECT_EQ(context.TryLock(), nullptr);
  }

  TEST_F(ExecutorContextTest, LifetimeTracking_ContextDoesNotExtendLifetime)
  {
    // Arrange
    auto executor = m_ioContext.get_executor();
    ExecutorContext<TestObject> context(std::make_shared<TestObject>(42), executor);

    // Act - Original shared_ptr is already out of scope

    // Assert - Context should not keep object alive (uses weak_ptr)
    EXPECT_FALSE(context.IsAlive());
    EXPECT_EQ(context.TryLock(), nullptr);
  }

  // ============================================================================
  // Multiple ExecutorContext Tests
  // ============================================================================

  TEST_F(ExecutorContextTest, MultipleContexts_SameObject_ShareLifetime)
  {
    // Arrange
    auto sharedPtr = std::make_shared<TestObject>(42);
    auto executor1 = m_ioContext.get_executor();
    boost::asio::io_context ioContext2;
    auto executor2 = ioContext2.get_executor();

    // Act - Create two contexts with same object
    ExecutorContext<TestObject> context1(sharedPtr, executor1);
    ExecutorContext<TestObject> context2(sharedPtr, executor2);

    // Assert - Both contexts see the object as alive
    EXPECT_TRUE(context1.IsAlive());
    EXPECT_TRUE(context2.IsAlive());

    // Act - Release shared reference
    sharedPtr.reset();

    // Assert - Both contexts now see object as dead
    EXPECT_FALSE(context1.IsAlive());
    EXPECT_FALSE(context2.IsAlive());
  }

  // ============================================================================
  // Different Template Type Tests
  // ============================================================================

  TEST_F(ExecutorContextTest, DifferentTypes_String_WorksCorrectly)
  {
    // Arrange
    auto sharedPtr = std::make_shared<std::string>("Hello, World!");
    auto executor = m_ioContext.get_executor();

    // Act
    ExecutorContext<std::string> context(sharedPtr, executor);

    // Assert
    EXPECT_TRUE(context.IsAlive());
    auto locked = context.TryLock();
    ASSERT_NE(locked, nullptr);
    EXPECT_EQ(*locked, "Hello, World!");
  }

  TEST_F(ExecutorContextTest, DifferentTypes_Int_WorksCorrectly)
  {
    // Arrange
    auto sharedPtr = std::make_shared<int>(12345);
    auto executor = m_ioContext.get_executor();

    // Act
    ExecutorContext<int> context(sharedPtr, executor);

    // Assert
    EXPECT_TRUE(context.IsAlive());
    auto locked = context.TryLock();
    ASSERT_NE(locked, nullptr);
    EXPECT_EQ(*locked, 12345);
  }

  // ============================================================================
  // Edge Cases
  // ============================================================================

  TEST_F(ExecutorContextTest, EdgeCase_ImmediateDestruction)
  {
    // Arrange & Act
    auto executor = m_ioContext.get_executor();
    ExecutorContext<TestObject> context(std::make_shared<TestObject>(42), executor);

    // Assert - shared_ptr was immediately destroyed after construction
    EXPECT_FALSE(context.IsAlive());
  }

  TEST_F(ExecutorContextTest, EdgeCase_MultipleIOContexts)
  {
    // Arrange
    boost::asio::io_context ioContext1;
    boost::asio::io_context ioContext2;
    auto sharedPtr = std::make_shared<TestObject>(42);
    auto executor1 = ioContext1.get_executor();
    auto executor2 = ioContext2.get_executor();

    // Act
    ExecutorContext<TestObject> context1(sharedPtr, executor1);
    ExecutorContext<TestObject> context2(sharedPtr, executor2);

    // Assert - Different executors, same object
    EXPECT_TRUE(context1.IsAlive());
    EXPECT_TRUE(context2.IsAlive());
    // Both contexts successfully store their respective executors
  }

}    // namespace Test2
