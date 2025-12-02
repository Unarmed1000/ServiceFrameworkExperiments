#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_PROCESSRESULT_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_PROCESSRESULT_HPP
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

#include <Test2/Framework/Service/ProcessStatus.hpp>
#include <chrono>
#include <stdexcept>

namespace Test2
{
  /// @brief Represents the result of a service Process() call, including status and optional sleep duration.
  struct ProcessResult
  {
    ProcessStatus Status{ProcessStatus::NoSleepLimit};
    std::chrono::nanoseconds Duration{};

    /// @brief Default constructor - creates NoSleepLimit result.
    constexpr ProcessResult() noexcept = default;

    /// @brief Construct with status only (duration defaults to zero).
    /// @param status The process status.
    constexpr explicit ProcessResult(ProcessStatus status) noexcept
      : Status(status)
      , Duration{}
    {
    }

    /// @brief Construct with status and duration.
    /// @param status The process status.
    /// @param duration The recommended sleep duration.
    constexpr ProcessResult(ProcessStatus status, std::chrono::nanoseconds duration) noexcept
      : Status(status)
      , Duration(duration)
    {
    }

    /// @brief Construct with duration only (implies SleepLimit status).
    /// @param duration The recommended max sleep duration.
    constexpr explicit ProcessResult(std::chrono::nanoseconds duration) noexcept
      : Status(ProcessStatus::SleepLimit)
      , Duration(duration)
    {
    }

    /// @brief Modify the result based on whether sleep is allowed.
    /// @param allowSleep If false and current status allows sleep, limits to a default max.
    /// @return Modified ProcessResult.
    [[nodiscard]] constexpr ProcessResult AllowSleep(bool allowSleep) const noexcept
    {
      if (!allowSleep)
      {
        constexpr auto defaultMaxSleepLimit = std::chrono::milliseconds(100);
        switch (Status)
        {
        case ProcessStatus::NoSleepLimit:
          return ProcessResult(ProcessStatus::SleepLimit, defaultMaxSleepLimit);
        case ProcessStatus::SleepLimit:
          return ProcessResult(ProcessStatus::SleepLimit, defaultMaxSleepLimit <= Duration ? defaultMaxSleepLimit : Duration);
        case ProcessStatus::Quit:
        default:
          break;
        }
      }
      return *this;
    }

    /// @brief Create a NoSleepLimit result.
    /// @return ProcessResult with NoSleepLimit status.
    [[nodiscard]] static constexpr ProcessResult NoSleepLimit() noexcept
    {
      return ProcessResult(ProcessStatus::NoSleepLimit);
    }

    /// @brief Create a Quit result.
    /// @return ProcessResult with Quit status.
    [[nodiscard]] static constexpr ProcessResult Quit() noexcept
    {
      return ProcessResult(ProcessStatus::Quit);
    }

    /// @brief Create a SleepLimit result with the specified duration.
    /// @param duration The recommended max sleep duration.
    /// @return ProcessResult with SleepLimit status.
    [[nodiscard]] static constexpr ProcessResult SleepLimit(std::chrono::nanoseconds duration) noexcept
    {
      return ProcessResult(ProcessStatus::SleepLimit, duration);
    }

    constexpr bool operator==(const ProcessResult& other) const noexcept = default;
  };


  /// @brief Merge two ProcessResult values, selecting the most restrictive result.
  ///
  /// Priority order: Quit > SleepLimit > NoSleepLimit
  /// For SleepLimit vs SleepLimit, the shorter duration wins.
  ///
  /// @param lhs First result.
  /// @param rhs Second result.
  /// @return The merged result.
  [[nodiscard]] constexpr ProcessResult Merge(const ProcessResult& lhs, const ProcessResult& rhs)
  {
    if (lhs.Status == ProcessStatus::NoSleepLimit)
    {
      return rhs;
    }

    if (lhs.Status == ProcessStatus::Quit)
    {
      return lhs;
    }

    // lhs.Status == ProcessStatus::SleepLimit
    switch (rhs.Status)
    {
    case ProcessStatus::NoSleepLimit:
      return lhs;
    case ProcessStatus::SleepLimit:
      return (lhs.Duration < rhs.Duration) ? lhs : rhs;
    case ProcessStatus::Quit:
      return rhs;
    default:
      throw std::invalid_argument("Could not merge the requested operation: unknown ProcessStatus");
    }
  }

  /// @brief Merge a ProcessResult with a ProcessStatus.
  /// @param result Existing result.
  /// @param status Status to merge.
  /// @return The merged result.
  [[nodiscard]] constexpr ProcessResult Merge(const ProcessResult& result, ProcessStatus status)
  {
    return Merge(result, ProcessResult(status));
  }
}

#endif
