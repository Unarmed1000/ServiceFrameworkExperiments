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

#include <Test2/Framework/Service/ProcessResult.hpp>
#include <Test2/Framework/Service/ProcessStatus.hpp>
#include <gtest/gtest.h>

namespace Test2
{
  using namespace std::chrono_literals;

  // ============================================================================
  // ProcessResult Constructor Tests
  // ============================================================================

  TEST(ProcessResult, DefaultConstructor_CreatesNoSleepLimit)
  {
    ProcessResult result;
    EXPECT_EQ(result.Status, ProcessStatus::NoSleepLimit);
    EXPECT_EQ(result.Duration, std::chrono::nanoseconds::zero());
  }

  TEST(ProcessResult, StatusOnlyConstructor_SetsStatusWithZeroDuration)
  {
    ProcessResult result(ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, std::chrono::nanoseconds::zero());
  }

  TEST(ProcessResult, StatusOnlyConstructor_Quit)
  {
    ProcessResult result(ProcessStatus::Quit);
    EXPECT_EQ(result.Status, ProcessStatus::Quit);
    EXPECT_EQ(result.Duration, std::chrono::nanoseconds::zero());
  }

  TEST(ProcessResult, StatusAndDurationConstructor)
  {
    auto duration = 500ms;
    ProcessResult result(ProcessStatus::SleepLimit, duration);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, duration);
  }

  TEST(ProcessResult, DurationOnlyConstructor_ImpliesSleepLimit)
  {
    auto duration = 250ms;
    ProcessResult result(duration);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, duration);
  }

  // ============================================================================
  // ProcessResult Static Factory Tests
  // ============================================================================

  TEST(ProcessResult, NoSleepLimitFactory)
  {
    auto result = ProcessResult::NoSleepLimit();
    EXPECT_EQ(result.Status, ProcessStatus::NoSleepLimit);
    EXPECT_EQ(result.Duration, std::chrono::nanoseconds::zero());
  }

  TEST(ProcessResult, QuitFactory)
  {
    auto result = ProcessResult::Quit();
    EXPECT_EQ(result.Status, ProcessStatus::Quit);
    EXPECT_EQ(result.Duration, std::chrono::nanoseconds::zero());
  }

  TEST(ProcessResult, SleepLimitFactory)
  {
    auto duration = 300ms;
    auto result = ProcessResult::SleepLimit(duration);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, duration);
  }

  // ============================================================================
  // ProcessResult AllowSleep Tests
  // ============================================================================

  TEST(ProcessResult, AllowSleep_TrueReturnsUnchanged)
  {
    auto original = ProcessResult::NoSleepLimit();
    auto result = original.AllowSleep(true);
    EXPECT_EQ(result, original);
  }

  TEST(ProcessResult, AllowSleep_TrueWithSleepLimitReturnsUnchanged)
  {
    auto original = ProcessResult::SleepLimit(50ms);
    auto result = original.AllowSleep(true);
    EXPECT_EQ(result, original);
  }

  TEST(ProcessResult, AllowSleep_FalseWithNoSleepLimit_AppliesDefault)
  {
    auto original = ProcessResult::NoSleepLimit();
    auto result = original.AllowSleep(false);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, 100ms);
  }

  TEST(ProcessResult, AllowSleep_FalseWithSleepLimitLongerThanDefault_AppliesDefault)
  {
    auto original = ProcessResult::SleepLimit(200ms);
    auto result = original.AllowSleep(false);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, 100ms);
  }

  TEST(ProcessResult, AllowSleep_FalseWithSleepLimitShorterThanDefault_KeepsShorter)
  {
    auto original = ProcessResult::SleepLimit(50ms);
    auto result = original.AllowSleep(false);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, 50ms);
  }

  TEST(ProcessResult, AllowSleep_FalseWithQuit_ReturnsQuit)
  {
    auto original = ProcessResult::Quit();
    auto result = original.AllowSleep(false);
    EXPECT_EQ(result.Status, ProcessStatus::Quit);
  }

  // ============================================================================
  // Merge Tests - NoSleepLimit as first argument
  // ============================================================================

  TEST(ProcessResult, Merge_NoSleepLimitWithNoSleepLimit_ReturnsNoSleepLimit)
  {
    auto result = Merge(ProcessResult::NoSleepLimit(), ProcessResult::NoSleepLimit());
    EXPECT_EQ(result.Status, ProcessStatus::NoSleepLimit);
  }

  TEST(ProcessResult, Merge_NoSleepLimitWithSleepLimit_ReturnsSleepLimit)
  {
    auto sleepLimit = ProcessResult::SleepLimit(150ms);
    auto result = Merge(ProcessResult::NoSleepLimit(), sleepLimit);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, 150ms);
  }

  TEST(ProcessResult, Merge_NoSleepLimitWithQuit_ReturnsQuit)
  {
    auto result = Merge(ProcessResult::NoSleepLimit(), ProcessResult::Quit());
    EXPECT_EQ(result.Status, ProcessStatus::Quit);
  }

  // ============================================================================
  // Merge Tests - SleepLimit as first argument
  // ============================================================================

  TEST(ProcessResult, Merge_SleepLimitWithNoSleepLimit_ReturnsSleepLimit)
  {
    auto sleepLimit = ProcessResult::SleepLimit(150ms);
    auto result = Merge(sleepLimit, ProcessResult::NoSleepLimit());
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, 150ms);
  }

  TEST(ProcessResult, Merge_SleepLimitWithShorterSleepLimit_ReturnsShorter)
  {
    auto longer = ProcessResult::SleepLimit(200ms);
    auto shorter = ProcessResult::SleepLimit(100ms);
    auto result = Merge(longer, shorter);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, 100ms);
  }

  TEST(ProcessResult, Merge_SleepLimitWithLongerSleepLimit_ReturnsShorter)
  {
    auto shorter = ProcessResult::SleepLimit(100ms);
    auto longer = ProcessResult::SleepLimit(200ms);
    auto result = Merge(shorter, longer);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, 100ms);
  }

  TEST(ProcessResult, Merge_SleepLimitWithQuit_ReturnsQuit)
  {
    auto sleepLimit = ProcessResult::SleepLimit(150ms);
    auto result = Merge(sleepLimit, ProcessResult::Quit());
    EXPECT_EQ(result.Status, ProcessStatus::Quit);
  }

  // ============================================================================
  // Merge Tests - Quit as first argument
  // ============================================================================

  TEST(ProcessResult, Merge_QuitWithNoSleepLimit_ReturnsQuit)
  {
    auto result = Merge(ProcessResult::Quit(), ProcessResult::NoSleepLimit());
    EXPECT_EQ(result.Status, ProcessStatus::Quit);
  }

  TEST(ProcessResult, Merge_QuitWithSleepLimit_ReturnsQuit)
  {
    auto result = Merge(ProcessResult::Quit(), ProcessResult::SleepLimit(100ms));
    EXPECT_EQ(result.Status, ProcessStatus::Quit);
  }

  TEST(ProcessResult, Merge_QuitWithQuit_ReturnsQuit)
  {
    auto result = Merge(ProcessResult::Quit(), ProcessResult::Quit());
    EXPECT_EQ(result.Status, ProcessStatus::Quit);
  }

  // ============================================================================
  // Merge with ProcessStatus overload
  // ============================================================================

  TEST(ProcessResult, Merge_ResultWithStatus_NoSleepLimit)
  {
    auto sleepLimit = ProcessResult::SleepLimit(100ms);
    auto result = Merge(sleepLimit, ProcessStatus::NoSleepLimit);
    EXPECT_EQ(result.Status, ProcessStatus::SleepLimit);
    EXPECT_EQ(result.Duration, 100ms);
  }

  TEST(ProcessResult, Merge_ResultWithStatus_Quit)
  {
    auto sleepLimit = ProcessResult::SleepLimit(100ms);
    auto result = Merge(sleepLimit, ProcessStatus::Quit);
    EXPECT_EQ(result.Status, ProcessStatus::Quit);
  }

  // ============================================================================
  // Equality Tests
  // ============================================================================

  TEST(ProcessResult, Equality_SameValues)
  {
    auto a = ProcessResult::SleepLimit(100ms);
    auto b = ProcessResult::SleepLimit(100ms);
    EXPECT_EQ(a, b);
  }

  TEST(ProcessResult, Equality_DifferentStatus)
  {
    auto a = ProcessResult::NoSleepLimit();
    auto b = ProcessResult::Quit();
    EXPECT_NE(a, b);
  }

  TEST(ProcessResult, Equality_DifferentDuration)
  {
    auto a = ProcessResult::SleepLimit(100ms);
    auto b = ProcessResult::SleepLimit(200ms);
    EXPECT_NE(a, b);
  }
}
