#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_EXCEPTION_DUPLICATESERVICEREGISTRATIONEXCEPTION_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_EXCEPTION_DUPLICATESERVICEREGISTRATIONEXCEPTION_HPP
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
  /// @brief Exception thrown when attempting to register a factory that has already been registered.
  ///
  /// This exception is thrown by ServiceRegistry::RegisterService when a factory of the same
  /// type (same typeid) has already been registered. Each factory type can only be registered once.
  class DuplicateServiceRegistrationException : public std::logic_error
  {
  public:
    explicit DuplicateServiceRegistrationException(const std::string& message)
      : std::logic_error(message)
    {
    }
  };
}

#endif
