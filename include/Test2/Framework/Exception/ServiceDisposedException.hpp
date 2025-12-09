#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_EXCEPTION_SERVICEDISPOSEDEXCEPTION_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_EXCEPTION_SERVICEDISPOSEDEXCEPTION_HPP
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

#include <fmt/format.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Test2
{
  /// @brief Exception thrown when a proxy's underlying service has been destroyed.
  ///
  /// This exception is thrown when attempting to access a service through a proxy
  /// after the underlying service instance has been destroyed (weak_ptr expired).
  class ServiceDisposedException : public std::runtime_error
  {
  public:
    /// @brief Constructs a ServiceDisposedException with an optional debug hint.
    /// @param debugHintName Optional name of the proxy for better diagnostics.
    ///                      If empty, uses a generic message.
    explicit ServiceDisposedException(std::string_view debugHintName)
      : std::runtime_error(debugHintName.empty() ? "Service proxy: target has been destroyed"
                                                 : fmt::format("{}: service has been destroyed", debugHintName))
    {
    }
  };
}

#endif
