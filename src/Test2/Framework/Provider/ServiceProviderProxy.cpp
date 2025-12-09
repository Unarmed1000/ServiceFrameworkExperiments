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
#include <Test2/Framework/Provider/ServiceProviderProxy.hpp>
#include <Test2/Framework/Service/IService.hpp>
#include <typeinfo>
#include <vector>

namespace Test2
{
  ServiceProviderProxy::ServiceProviderProxy(std::shared_ptr<IServiceProvider> provider)
    : m_provider(std::move(provider))
  {
  }

  void ServiceProviderProxy::Clear()
  {
    m_provider.reset();
  }

  std::shared_ptr<IService> ServiceProviderProxy::GetService(const std::type_info& type) const
  {
    if (!m_provider)
    {
      throw ServiceProviderException("ServiceProvider has been cleared");
    }
    return m_provider->GetService(type);
  }

  std::shared_ptr<IService> ServiceProviderProxy::TryGetService(const std::type_info& type) const
  {
    if (!m_provider)
    {
      return nullptr;
    }
    return m_provider->TryGetService(type);
  }

  bool ServiceProviderProxy::TryGetServices(const std::type_info& type, std::vector<std::shared_ptr<IService>>& rServices) const
  {
    if (!m_provider)
    {
      return false;
    }
    return m_provider->TryGetServices(type, rServices);
  }
}
