#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_SUBTRACT_SUBTRACTSERVICEFACTORY_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_SUBTRACT_SUBTRACTSERVICEFACTORY_HPP
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
#include <Test2/Services/Subtract/ISubtractService.hpp>
#include <Test2/Services/Subtract/SubtractService.hpp>
#include <memory>
#include <span>
#include <stdexcept>
#include <typeindex>

namespace Test2
{
  /// @brief Factory for creating SubtractService instances.
  class SubtractServiceFactory final : public IServiceFactory
  {
  public:
    SubtractServiceFactory() = default;
    ~SubtractServiceFactory() override = default;

    std::span<const std::type_index> GetSupportedInterfaces() const override
    {
      static const std::type_index interfaces[] = {std::type_index(typeid(ISubtractService))};
      return std::span<const std::type_index>(interfaces);
    }

    std::shared_ptr<IServiceControl> Create(const std::type_index& type, const ServiceCreateInfo& createInfo) override
    {
      if (type == std::type_index(typeid(ISubtractService)))
      {
        return std::make_shared<SubtractService>(createInfo);
      }
      throw std::invalid_argument("SubtractServiceFactory: unsupported interface type");
    }
  };

}

#endif
