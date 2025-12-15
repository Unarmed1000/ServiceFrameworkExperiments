#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_PROVIDER_SERVICEPROVIDERSERVICEINSTANCE_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_PROVIDER_SERVICEPROVIDERSERVICEINSTANCE_HPP
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

#include <Test2/Framework/Service/IServiceControlBase.hpp>
#include <memory>
#include <typeindex>
#include <vector>

namespace Test2
{
  /// @brief Distinguishes between service and proxy instances.
  enum class InstanceType
  {
    Service,
    Proxy
  };

  /// @brief Internal service provider instance storage.
  ///
  /// Stores a service or proxy control interface along with the list of interfaces
  /// that this instance supports for type-based lookup.
  /// This is used internally by the service provider to track both services and proxies.
  struct ServiceProviderServiceInstance
  {
    InstanceType Type;
    std::shared_ptr<IServiceControlBase> Instance;
    std::vector<std::type_index> SupportedInterfaces;
  };
}

#endif
