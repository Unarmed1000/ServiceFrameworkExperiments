#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_CALCULATOR_CALCULATORSERVICE_PROXY_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_CALCULATOR_CALCULATORSERVICE_PROXY_HPP
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

#include <Test2/Framework/Lifecycle/DispatchContext.hpp>
#include <Test2/Framework/Lifecycle/ILifeTracker.hpp>
#include <Test2/Framework/Service/Async/AsyncServiceProxyBase.hpp>
#include <Test2/Services/Calculator/ICalculatorService.hpp>
#include "CalculatorService.hpp"

namespace Test2
{
  /// @brief Proxy for CalculatorService that handles cross-thread dispatching.
  class CalculatorServiceProxy final
    : public AsyncServiceProxyBase
    , public ICalculatorService
  {
    ///! Dispatch context containing source and target executor contexts.
    DispatchContext<ILifeTracker, CalculatorService> m_dispatchContext;

  public:
    explicit CalculatorServiceProxy(const ServiceProxyCreateInfo& createInfo);

    /// See ICalculatorService
    boost::asio::awaitable<double> EvaluateAsync(std::string expression) final;
  };

}

#endif
