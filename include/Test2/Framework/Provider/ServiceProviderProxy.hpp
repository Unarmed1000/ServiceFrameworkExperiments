#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_PROVIDER_SERVICEPROVIDERPROXY_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_PROVIDER_SERVICEPROVIDERPROXY_HPP
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

#include <Test2/Framework/Exception/ServiceProviderException.hpp>
#include <Test2/Framework/Provider/IServiceProvider.hpp>
#include <memory>

namespace Test2
{
  /// @brief A proxy wrapper for IServiceProvider that can be cleared to prevent further service access.
  ///
  /// This class acts as a proxy between services and the actual service provider. It allows
  /// the provider to be "cleared" (disconnected), after which all service access attempts will
  /// fail gracefully. This is useful for preventing services from accessing the provider after
  /// initialization failures or during shutdown sequences.
  class ServiceProviderProxy : public IServiceProvider
  {
    std::shared_ptr<IServiceProvider> m_provider;

  public:
    /// @brief Constructs a proxy wrapping the given service provider.
    /// @param provider The underlying service provider to wrap.
    explicit ServiceProviderProxy(std::shared_ptr<IServiceProvider> provider)
      : m_provider(std::move(provider))
    {
    }

    /// @brief Clears the internal provider pointer, disconnecting the proxy.
    ///
    /// After calling this method, all service access attempts through this proxy
    /// will fail (throw exceptions or return null/false).
    void Clear()
    {
      m_provider.reset();
    }

    // IServiceProvider interface implementations

    std::shared_ptr<IService> GetService(const std::type_info& type) const override
    {
      if (!m_provider)
      {
        throw ServiceProviderException("ServiceProvider has been cleared");
      }
      return m_provider->GetService(type);
    }

    std::shared_ptr<IService> TryGetService(const std::type_info& type) const override
    {
      if (!m_provider)
      {
        return nullptr;
      }
      return m_provider->TryGetService(type);
    }

    bool TryGetServices(const std::type_info& type, std::vector<std::shared_ptr<IService>>& rServices) const override
    {
      if (!m_provider)
      {
        return false;
      }
      return m_provider->TryGetServices(type, rServices);
    }
  };
}

#endif
