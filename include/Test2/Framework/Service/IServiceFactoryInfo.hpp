#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ISERVICEFACTORYINFO_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ISERVICEFACTORYINFO_HPP
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

#include <span>
#include <typeindex>

namespace Test2
{
  class IServiceFactoryInfo
  {
  public:
    virtual ~IServiceFactoryInfo() = default;

    /// @brief Retrieves the list of service interface types that this factory can create.
    ///
    /// This method returns a span of type_index objects representing all
    /// the service interfaces that this factory supports. The framework uses this information
    /// to determine which factory to use when a specific service type is requested.
    ///
    /// @return A span of type_index objects for the supported interface types.
    virtual std::span<const std::type_index> GetSupportedInterfaces() const = 0;
  };

}

#endif