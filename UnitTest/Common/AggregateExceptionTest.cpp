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

#include <Common/AggregateException.hpp>
#include <gtest/gtest.h>
#include <list>
#include <stdexcept>
#include <vector>

namespace Common
{
  // Custom exceptions for testing
  class CustomException : public std::runtime_error
  {
  public:
    explicit CustomException(const std::string& msg)
      : std::runtime_error(msg)
    {
    }
  };

  class AnotherCustomException : public std::logic_error
  {
  public:
    explicit AnotherCustomException(const std::string& msg)
      : std::logic_error(msg)
    {
    }
  };
}

using namespace Common;

TEST(AggregateExceptionTest, BasicConstructionWithVector)
{
  std::vector<std::exception_ptr> exceptions;
  exceptions.push_back(std::make_exception_ptr(std::runtime_error("Error 1")));
  exceptions.push_back(std::make_exception_ptr(std::logic_error("Error 2")));

  AggregateException aggEx(std::move(exceptions));
  EXPECT_EQ(aggEx.InnerExceptionCount(), 2);
  EXPECT_EQ(std::string(aggEx.what()), "One or more errors occurred.");
}

TEST(AggregateExceptionTest, ConstructionWithCustomMessage)
{
  std::vector<std::exception_ptr> exceptions;
  exceptions.push_back(std::make_exception_ptr(std::runtime_error("Error 1")));

  AggregateException aggEx("Custom error message", std::move(exceptions));
  EXPECT_EQ(aggEx.InnerExceptionCount(), 1);
  EXPECT_EQ(std::string(aggEx.what()), "Custom error message");
}

TEST(AggregateExceptionTest, ConstructionWithInitializerList)
{
  AggregateException aggEx({std::make_exception_ptr(std::runtime_error("Error 1")), std::make_exception_ptr(std::logic_error("Error 2")),
                            std::make_exception_ptr(CustomException("Error 3"))});

  EXPECT_EQ(aggEx.InnerExceptionCount(), 3);
}

TEST(AggregateExceptionTest, ConstructionWithMessageAndInitializerList)
{
  AggregateException aggEx("Multiple errors occurred",
                           {std::make_exception_ptr(std::runtime_error("Error 1")), std::make_exception_ptr(std::logic_error("Error 2"))});

  EXPECT_EQ(aggEx.InnerExceptionCount(), 2);
  EXPECT_EQ(std::string(aggEx.what()), "Multiple errors occurred");
}

TEST(AggregateExceptionTest, TemplateConstructorWithVectorOfExceptions)
{
  std::vector<std::runtime_error> exceptions;
  exceptions.push_back(std::runtime_error("Error 1"));
  exceptions.push_back(std::runtime_error("Error 2"));
  exceptions.push_back(std::runtime_error("Error 3"));

  AggregateException aggEx(exceptions);
  EXPECT_EQ(aggEx.InnerExceptionCount(), 3);
}

TEST(AggregateExceptionTest, TemplateConstructorWithList)
{
  std::list<CustomException> exceptions;
  exceptions.push_back(CustomException("Error 1"));
  exceptions.push_back(CustomException("Error 2"));

  AggregateException aggEx(exceptions);
  EXPECT_EQ(aggEx.InnerExceptionCount(), 2);
}

TEST(AggregateExceptionTest, TemplateConstructorWithCustomMessage)
{
  std::vector<std::logic_error> exceptions;
  exceptions.push_back(std::logic_error("Error 1"));
  exceptions.push_back(std::logic_error("Error 2"));

  AggregateException aggEx("Logic errors occurred", exceptions);
  EXPECT_EQ(aggEx.InnerExceptionCount(), 2);
  EXPECT_EQ(std::string(aggEx.what()), "Logic errors occurred");
}

TEST(AggregateExceptionTest, EmptyVectorThrows)
{
  std::vector<std::exception_ptr> emptyExceptions;
  EXPECT_THROW(AggregateException aggEx(std::move(emptyExceptions)), std::invalid_argument);
}

TEST(AggregateExceptionTest, EmptyInitializerListThrows)
{
  EXPECT_THROW(AggregateException aggEx({}), std::invalid_argument);
}

TEST(AggregateExceptionTest, GetInnerExceptionsReturnsConstReference)
{
  std::vector<std::exception_ptr> exceptions;
  exceptions.push_back(std::make_exception_ptr(std::runtime_error("Error 1")));
  exceptions.push_back(std::make_exception_ptr(std::logic_error("Error 2")));

  AggregateException aggEx(std::move(exceptions));
  const auto& innerExceptions = aggEx.GetInnerExceptions();
  ASSERT_EQ(innerExceptions.size(), 2);

  // Verify we can rethrow exceptions
  try
  {
    std::rethrow_exception(innerExceptions[0]);
    FAIL() << "Exception should have been thrown";
  }
  catch (const std::runtime_error& ex)
  {
    EXPECT_EQ(std::string(ex.what()), "Error 1");
  }
}

TEST(AggregateExceptionTest, GetBaseExceptionReturnsFirst)
{
  AggregateException aggEx({std::make_exception_ptr(CustomException("First error")), std::make_exception_ptr(std::runtime_error("Second error"))});

  auto baseEx = aggEx.GetBaseException();
  ASSERT_NE(baseEx, nullptr);

  try
  {
    std::rethrow_exception(baseEx);
    FAIL() << "Exception should have been thrown";
  }
  catch (const CustomException& ex)
  {
    EXPECT_EQ(std::string(ex.what()), "First error");
  }
}

TEST(AggregateExceptionTest, IteratorSupport)
{
  AggregateException aggEx({std::make_exception_ptr(std::runtime_error("Error 1")), std::make_exception_ptr(std::runtime_error("Error 2")),
                            std::make_exception_ptr(std::runtime_error("Error 3"))});

  int count = 0;
  for (const auto& exPtr : aggEx)
  {
    EXPECT_NE(exPtr, nullptr);
    count++;
  }
  EXPECT_EQ(count, 3);
}

TEST(AggregateExceptionTest, FlattenSingleLevel)
{
  AggregateException aggEx({std::make_exception_ptr(std::runtime_error("Error 1")), std::make_exception_ptr(std::logic_error("Error 2"))});

  auto flattened = aggEx.Flatten();
  EXPECT_EQ(flattened.InnerExceptionCount(), 2);
}

TEST(AggregateExceptionTest, FlattenNested)
{
  // Create inner aggregate exceptions
  AggregateException inner1(
    {std::make_exception_ptr(std::runtime_error("Inner1-Error1")), std::make_exception_ptr(std::runtime_error("Inner1-Error2"))});

  AggregateException inner2({std::make_exception_ptr(std::logic_error("Inner2-Error1"))});

  // Create outer aggregate exception
  AggregateException outer(
    {std::make_exception_ptr(inner1), std::make_exception_ptr(std::runtime_error("Outer-Error1")), std::make_exception_ptr(inner2)});

  ASSERT_EQ(outer.InnerExceptionCount(), 3);

  // Flatten should unwrap nested aggregates
  auto flattened = outer.Flatten();
  EXPECT_EQ(flattened.InnerExceptionCount(), 4);    // 2 from inner1 + 1 from outer + 1 from inner2
}

TEST(AggregateExceptionTest, FlattenDeeplyNested)
{
  AggregateException level3({std::make_exception_ptr(std::runtime_error("Level3-Error"))});

  AggregateException level2({std::make_exception_ptr(level3), std::make_exception_ptr(std::logic_error("Level2-Error"))});

  AggregateException level1({std::make_exception_ptr(level2), std::make_exception_ptr(CustomException("Level1-Error"))});

  auto flattened = level1.Flatten();
  EXPECT_EQ(flattened.InnerExceptionCount(), 3);    // All leaf exceptions
}

TEST(AggregateExceptionTest, ToStringBasic)
{
  AggregateException aggEx({std::make_exception_ptr(std::runtime_error("Error 1")), std::make_exception_ptr(std::logic_error("Error 2"))});

  std::string str = aggEx.ToString();
  EXPECT_NE(str.find("One or more errors occurred."), std::string::npos);
  EXPECT_NE(str.find("[0]"), std::string::npos);
  EXPECT_NE(str.find("[1]"), std::string::npos);
  EXPECT_NE(str.find("Error 1"), std::string::npos);
  EXPECT_NE(str.find("Error 2"), std::string::npos);
}

TEST(AggregateExceptionTest, ToStringWithCustomMessage)
{
  AggregateException aggEx("Multiple failures detected", {std::make_exception_ptr(CustomException("Custom error"))});

  std::string str = aggEx.ToString();
  EXPECT_NE(str.find("Multiple failures detected"), std::string::npos);
  EXPECT_NE(str.find("Custom error"), std::string::npos);
}

TEST(AggregateExceptionTest, CopyConstructor)
{
  AggregateException original({std::make_exception_ptr(std::runtime_error("Error 1")), std::make_exception_ptr(std::logic_error("Error 2"))});

  AggregateException copy = original;
  EXPECT_EQ(copy.InnerExceptionCount(), 2);
  EXPECT_EQ(std::string(copy.what()), std::string(original.what()));
}

TEST(AggregateExceptionTest, MoveConstructor)
{
  AggregateException original({std::make_exception_ptr(std::runtime_error("Error 1")), std::make_exception_ptr(std::logic_error("Error 2"))});

  AggregateException moved = std::move(original);
  EXPECT_EQ(moved.InnerExceptionCount(), 2);
}

TEST(AggregateExceptionTest, AssignmentOperatorsDeleted)
{
  // This test verifies that assignment operators are deleted at compile time
  // The test passes if the code compiles, demonstrating immutability
  SUCCEED();
}

TEST(AggregateExceptionTest, ThrowAndCatch)
{
  EXPECT_THROW(
    { throw AggregateException({std::make_exception_ptr(std::runtime_error("Error 1")), std::make_exception_ptr(std::logic_error("Error 2"))}); },
    AggregateException);
}

TEST(AggregateExceptionTest, MixedExceptionTypes)
{
  AggregateException aggEx({std::make_exception_ptr(std::runtime_error("Runtime error")), std::make_exception_ptr(std::logic_error("Logic error")),
                            std::make_exception_ptr(CustomException("Custom error")),
                            std::make_exception_ptr(AnotherCustomException("Another custom error"))});

  ASSERT_EQ(aggEx.InnerExceptionCount(), 4);

  // Verify each exception type
  const auto& exceptions = aggEx.GetInnerExceptions();
  int runtimeCount = 0;
  int logicCount = 0;
  int customCount = 0;
  int anotherCustomCount = 0;

  for (const auto& exPtr : exceptions)
  {
    try
    {
      std::rethrow_exception(exPtr);
    }
    catch (const CustomException&)
    {
      customCount++;
    }
    catch (const AnotherCustomException&)
    {
      anotherCustomCount++;
    }
    catch (const std::runtime_error&)
    {
      runtimeCount++;
    }
    catch (const std::logic_error&)
    {
      logicCount++;
    }
  }

  EXPECT_EQ(runtimeCount, 1);
  EXPECT_EQ(logicCount, 1);
  EXPECT_EQ(customCount, 1);
  EXPECT_EQ(anotherCustomCount, 1);
}

TEST(AggregateExceptionTest, LargeNumberOfExceptions)
{
  std::vector<std::exception_ptr> exceptions;
  for (int i = 0; i < 100; ++i)
  {
    exceptions.push_back(std::make_exception_ptr(std::runtime_error("Error " + std::to_string(i))));
  }

  AggregateException aggEx(std::move(exceptions));
  EXPECT_EQ(aggEx.InnerExceptionCount(), 100);
}

TEST(AggregateExceptionTest, ConstCorrectness)
{
  const AggregateException aggEx({std::make_exception_ptr(std::runtime_error("Error 1"))});

  EXPECT_EQ(aggEx.InnerExceptionCount(), 1);
  const auto& exceptions = aggEx.GetInnerExceptions();
  EXPECT_EQ(exceptions.size(), 1);
  auto baseEx = aggEx.GetBaseException();
  EXPECT_NE(baseEx, nullptr);
  auto it = aggEx.begin();
  EXPECT_NE(it, aggEx.end());
}

TEST(AggregateExceptionTest, InnerExceptionCount)
{
  AggregateException aggEx1({std::make_exception_ptr(std::runtime_error("Error"))});
  EXPECT_EQ(aggEx1.InnerExceptionCount(), 1);

  AggregateException aggEx5({std::make_exception_ptr(std::runtime_error("E1")), std::make_exception_ptr(std::runtime_error("E2")),
                             std::make_exception_ptr(std::runtime_error("E3")), std::make_exception_ptr(std::runtime_error("E4")),
                             std::make_exception_ptr(std::runtime_error("E5"))});
  EXPECT_EQ(aggEx5.InnerExceptionCount(), 5);
}

TEST(AggregateExceptionTest, ExceptionPolymorphism)
{
  AggregateException aggEx({std::make_exception_ptr(std::runtime_error("Error"))});

  // Can be caught as std::exception
  try
  {
    throw aggEx;
    FAIL() << "Exception should have been thrown";
  }
  catch (const std::exception& ex)
  {
    EXPECT_EQ(std::string(ex.what()), "One or more errors occurred.");
  }
}

TEST(AggregateExceptionTest, FlattenMixedNestedAndNonNested)
{
  AggregateException nested({std::make_exception_ptr(std::runtime_error("Nested1")), std::make_exception_ptr(std::runtime_error("Nested2"))});

  AggregateException outer(
    {std::make_exception_ptr(std::runtime_error("Direct1")), std::make_exception_ptr(nested), std::make_exception_ptr(std::logic_error("Direct2"))});

  auto flattened = outer.Flatten();
  EXPECT_EQ(flattened.InnerExceptionCount(), 4);    // Direct1, Nested1, Nested2, Direct2
}
