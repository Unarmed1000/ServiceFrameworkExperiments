#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_REGISTRY_SERVICETHREADGROUPID_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_REGISTRY_SERVICETHREADGROUPID_HPP
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
  /// @brief Represents a thread group identifier for organizing service execution contexts.
  ///
  /// ServiceThreadGroupId is used to assign services to specific thread groups within the framework.
  /// Services in the same thread group may share execution resources, coordination mechanisms, or
  /// execution contexts. The numeric identifier itself has no intrinsic meaning - it's simply used
  /// to group services together.
  ///
  /// Thread group identifiers are typically created by the framework using
  /// IServiceRegistry::CreateServiceThreadGroupId() to ensure uniqueness.
  ///
  /// @note The default-constructed value (0) represents an unassigned or default thread group.
  class ServiceThreadGroupId
  {
  private:
    /// @brief The numeric identifier for the thread group.
    /// The value itself has no special meaning - it's used only for grouping.
    uint32_t m_groupId{0};

  public:
    /// @brief Default constructor. Initializes to group ID 0 (default/unassigned group).
    constexpr ServiceThreadGroupId() noexcept = default;

    /// @brief Constructs a ServiceThreadGroupId with the specified group identifier.
    ///
    /// @param groupId The numeric identifier for this thread group. Should typically be
    ///                obtained from IServiceRegistry::CreateServiceThreadGroupId().
    explicit constexpr ServiceThreadGroupId(const uint32_t groupId) noexcept
      : m_groupId(groupId)
    {
    }

    /// @brief Gets the underlying thread group identifier value.
    ///
    /// @return The thread group ID as an unsigned 32-bit integer.
    [[nodiscard]] constexpr uint32_t GetValue() const noexcept
    {
      return m_groupId;
    }

    /// @brief Three-way comparison operator for thread group ordering.
    constexpr auto operator<=>(const ServiceThreadGroupId& other) const noexcept = default;

    /// @brief Equality comparison operator.
    constexpr bool operator==(const ServiceThreadGroupId& other) const noexcept = default;
  };

}

#endif