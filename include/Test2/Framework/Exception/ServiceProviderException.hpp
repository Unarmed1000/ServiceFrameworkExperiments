#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_EXCEPTION_SERVICEPROVIDEREXCEPTION_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_EXCEPTION_SERVICEPROVIDEREXCEPTION_HPP
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

#include <stdexcept>
#include <string>

namespace Test2
{
  /// @brief Exception thrown when a service provider operation fails.
  ///
  /// This exception is thrown when a service provider encounters an error condition,
  /// such as when a proxy has been cleared or when accessing from the wrong thread.
  class ServiceProviderException : public std::runtime_error
  {
  public:
    explicit ServiceProviderException(const std::string& message)
      : std::runtime_error(message)
    {
    }
  };
}

#endif
