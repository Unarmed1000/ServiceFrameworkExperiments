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

#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace Test2
{
  ServiceProvider::ServiceProvider(std::weak_ptr<IServiceProvider> provider)
    : m_provider(std::move(provider))
  {
  }

  std::shared_ptr<IService> ServiceProvider::GetService(const std::type_info& type) const
  {
    auto provider = m_provider.lock();
    if (!provider)
    {
      spdlog::error("ServiceProvider::GetService: underlying IServiceProvider has been destroyed");
      throw std::runtime_error("ServiceProvider: underlying IServiceProvider has been destroyed");
    }
    return provider->GetService(type);
  }

  std::shared_ptr<IService> ServiceProvider::TryGetService(const std::type_info& type) const
  {
    auto provider = m_provider.lock();
    if (!provider)
    {
      spdlog::warn("ServiceProvider::TryGetService: underlying IServiceProvider has been destroyed");
      return nullptr;
    }
    return provider->TryGetService(type);
  }

  bool ServiceProvider::TryGetServices(const std::type_info& type, std::vector<std::shared_ptr<IService>>& rServices) const
  {
    auto provider = m_provider.lock();
    if (!provider)
    {
      spdlog::warn("ServiceProvider::TryGetServices: underlying IServiceProvider has been destroyed");
      return false;
    }
    return provider->TryGetServices(type, rServices);
  }
}
