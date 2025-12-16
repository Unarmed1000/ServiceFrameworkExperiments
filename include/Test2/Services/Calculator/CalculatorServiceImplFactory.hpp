#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_CALCULATOR_CALCULATORSERVICEIMPLFACTORY_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_CALCULATOR_CALCULATORSERVICEIMPLFACTORY_HPP
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

#include <Test2/Framework/Service/Async/AsyncServiceImplFactory.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <Test2/Services/Calculator/ICalculatorService.hpp>
#include <memory>

namespace Test2
{
  /// @brief Factory for creating CalculatorService implementations.
  class CalculatorServiceImplFactory final : public AsyncServiceImplFactory
  {
  public:
    CalculatorServiceImplFactory();
    ~CalculatorServiceImplFactory() override = default;

    std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& createInfo) override;
  };

}

#endif
