#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_MULTIPLY_MULTIPLYSERVICE_PROXY_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_MULTIPLY_MULTIPLYSERVICE_PROXY_HPP
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
#include <Test2/Services/Multiply/IMultiplyService.hpp>
#include "MultiplyService.hpp"

namespace Test2
{
  /// @brief Proxy for MultiplyService that handles cross-thread dispatching.
  class MultiplyServiceProxy final
    : public AsyncServiceProxyBase
    , public IMultiplyService
  {
    ///! Dispatch context containing source and target executor contexts.
    DispatchContext<ILifeTracker, MultiplyService> m_dispatchContext;

  public:
    explicit MultiplyServiceProxy(const ServiceProxyCreateInfo& createInfo);

    /// See IMultiplyService
    boost::asio::awaitable<double> MultiplyAsync(double a, double b) final;
  };

}

#endif
