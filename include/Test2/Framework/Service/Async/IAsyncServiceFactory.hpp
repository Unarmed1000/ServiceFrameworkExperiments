#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_IASYNCSERVICEFACTORY_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_IASYNCSERVICEFACTORY_HPP
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

#include <Test2/Framework/Service/Async/IAsyncServiceImplFactory.hpp>
#include <Test2/Framework/Service/Async/IAsyncServiceProxyFactory.hpp>

namespace Test2
{
  /// @brief Interface for asynchronous service factories that provide both proxy and implementation creation.
  ///
  /// The IAsyncServiceFactory interface combines both proxy and implementation factory capabilities
  /// for asynchronous services. This unified interface allows the framework to create both the
  /// service implementation and its corresponding proxies for cross-thread communication.
  class IAsyncServiceFactory
    : public virtual IAsyncServiceProxyFactory
    , public virtual IAsyncServiceImplFactory
  {
  public:
    virtual ~IAsyncServiceFactory() = default;

    /// @brief Gets the type identifier for the implementation factory.
    ///
    /// This method returns a type_index that uniquely identifies the implementation factory type,
    /// which is used for duplicate detection during service registration.
    ///
    /// @return The type_index of the implementation factory.
    virtual std::type_index GetImplFactoryTypeId() const = 0;
  };

}

#endif
