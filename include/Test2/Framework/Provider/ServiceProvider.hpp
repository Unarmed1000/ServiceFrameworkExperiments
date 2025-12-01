#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_PROVIDER_SERVICEPROVIDER_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_PROVIDER_SERVICEPROVIDER_HPP
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

#include <Test2/Framework/Provider/IServiceProvider.hpp>
#include <memory>
#include <vector>

namespace Test2
{
  class ServiceProvider
  {
    std::weak_ptr<IServiceProvider> m_provider;

  public:
    explicit ServiceProvider(std::weak_ptr<IServiceProvider> provider);

    std::shared_ptr<IService> GetService(const std::type_info& type) const;
    std::shared_ptr<IService> TryGetService(const std::type_info& type) const;
    bool TryGetServices(const std::type_info& type, std::vector<std::shared_ptr<IService>>& rServices) const;
  };
}

#endif