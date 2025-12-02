#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_DIVIDE_DIVIDESERVICEFACTORY_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_DIVIDE_DIVIDESERVICEFACTORY_HPP
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

#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <Test2/Services/Divide/DivideService.hpp>
#include <Test2/Services/Divide/IDivideService.hpp>
#include <memory>
#include <span>
#include <stdexcept>

namespace Test2
{
  /// @brief Factory for creating DivideService instances.
  class DivideServiceFactory final : public IServiceFactory
  {
  public:
    DivideServiceFactory() = default;
    ~DivideServiceFactory() override = default;

    std::span<const std::type_info> GetSupportedInterfaces() const override
    {
      static const std::type_info* interfaces[] = {&typeid(IDivideService)};
      return std::span<const std::type_info>(reinterpret_cast<const std::type_info*>(interfaces), 1);
    }

    std::shared_ptr<IServiceControl> Create(const std::type_info& type, const ServiceCreateInfo& createInfo) override
    {
      if (type == typeid(IDivideService))
      {
        return std::make_shared<DivideService>(createInfo);
      }
      throw std::invalid_argument("DivideServiceFactory: unsupported interface type");
    }
  };

}

#endif
