#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_REGISTRY_SERVICELAUNCHPRIORITY_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_REGISTRY_SERVICELAUNCHPRIORITY_HPP
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

#include <compare>
#include <cstdint>

namespace Test2
{
  /// @brief Represents the launch priority of a service in the framework.
  ///
  /// ServiceLaunchPriority determines the initialization order of services during framework
  /// startup. Higher priority values are launched first. This priority also controls service
  /// dependency access - a service can only access other services with higher priority values,
  /// ensuring proper dependency ordering and preventing circular dependencies.
  ///
  /// @note Priority values are arbitrary uint32_t values. Consider using ranges like:
  ///       - 1000+ for critical infrastructure services
  ///       - 500-999 for core services
  ///       - 100-499 for standard services
  ///       - 0-99 for low-priority services
  class ServiceLaunchPriority
  {
  private:
    uint32_t m_priority{0};

  public:
    /// @brief Default constructor. Initializes priority to 0 (lowest priority).
    constexpr ServiceLaunchPriority() noexcept = default;

    /// @brief Constructs a ServiceLaunchPriority with the specified priority value.
    ///
    /// @param priority The priority value. Higher values launch first and can be accessed
    ///                 as dependencies by lower-priority services.
    explicit constexpr ServiceLaunchPriority(const uint32_t priority) noexcept
      : m_priority(priority)
    {
    }

    /// @brief Gets the underlying priority value.
    ///
    /// @return The priority value as an unsigned 32-bit integer.
    [[nodiscard]] constexpr uint32_t GetValue() const noexcept
    {
      return m_priority;
    }

    /// @brief Three-way comparison operator for priority ordering.
    constexpr auto operator<=>(const ServiceLaunchPriority& other) const noexcept = default;

    /// @brief Equality comparison operator.
    constexpr bool operator==(const ServiceLaunchPriority& other) const noexcept = default;
  };

}

#endif