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

#include <fmt/format.h>
#include <stdexcept>
#include <string>
#include <typeinfo>

namespace Test2
{
  /// @brief Exception thrown when a requested service is not found.
  ///
  /// This exception is thrown by IServiceProvider::GetService when no service
  /// matching the requested type information is registered.
  class UnknownServiceException : public std::runtime_error
  {
  public:
    explicit UnknownServiceException(const std::string& message)
      : std::runtime_error(message)
    {
    }
  };

  /// @brief Exception thrown when multiple services are found but only one was expected.
  ///
  /// This exception is thrown by IServiceProvider::GetService when more than one service
  /// matches the requested type information. Use TryGetServices to retrieve all matching services.
  class MultipleServicesFoundException : public std::runtime_error
  {
  public:
    explicit MultipleServicesFoundException(const std::string& message)
      : std::runtime_error(message)
    {
    }
  };

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

  /// @brief Exception thrown when a service cast fails.
  ///
  /// This exception is thrown by ServiceProvider::GetService<T>() when a service is found
  /// but cannot be cast to the requested type T via dynamic_pointer_cast. This indicates
  /// a fundamental type mismatch between the registered service and the requested interface,
  /// which typically suggests a configuration or registration error.
  ///
  /// Inherits from std::bad_cast to allow catching as either ServiceCastException or std::bad_cast.
  ///
  /// The exception message includes both the requested type name and the actual runtime type
  /// of the service that was found, to aid in debugging.
  ///
  /// @note Type names are compiler-specific (mangled on GCC/Clang) but sufficient for debugging.
  class ServiceCastException : public std::bad_cast
  {
    std::string m_message;

  public:
    /// @brief Constructs a ServiceCastException with type information.
    /// @param requestedTypeName The name of the type that was requested (typically from typeid(T).name()).
    /// @param actualTypeName The name of the actual runtime type of the service (typically from typeid(*service).name()).
    ServiceCastException(const std::string& requestedTypeName, const std::string& actualTypeName)
      : m_message(fmt::format("ServiceCastException: Failed to cast service from '{}' to requested type '{}'", actualTypeName, requestedTypeName))
    {
    }

    /// @brief Returns a descriptive error message.
    /// @return A C-string containing the error message with both type names.
    [[nodiscard]] const char* what() const noexcept override
    {
      return m_message.c_str();
    }
  };
}

#endif
