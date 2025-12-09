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

#include <Test2/Framework/Host/ServiceHostProxy.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Util/AsyncProxyHelper.hpp>
#include "ServiceHostBase.hpp"

namespace Test2
{
  inline constexpr const char kProxyName[] = "ServiceHostProxy";

  ServiceHostProxy::ServiceHostProxy(Lifecycle::ExecutorContext<ServiceHostBase> context)
    : m_context(std::move(context))
  {
  }

  ServiceHostProxy::~ServiceHostProxy() = default;


  boost::asio::awaitable<void> ServiceHostProxy::TryStartServicesAsync(std::vector<StartServiceRecord> services,
                                                                       const ServiceLaunchPriority currentPriority)
  {
    co_await Util::InvokeAsync<kProxyName>(m_context, &ServiceHostBase::TryStartServicesAsync, std::move(services), currentPriority);
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>> ServiceHostProxy::TryShutdownServicesAsync(const ServiceLaunchPriority priority)
  {
    co_return co_await Util::InvokeAsync<kProxyName>(m_context, &ServiceHostBase::TryShutdownServicesAsync, priority);
  }

  boost::asio::awaitable<bool> ServiceHostProxy::TryRequestShutdownAsync()
  {
    co_return co_await Util::TryInvokeAsync<kProxyName>(m_context, &ServiceHostBase::RequestShutdown);
  }

  bool ServiceHostProxy::TryRequestShutdown() noexcept
  {
    return Util::TryInvokePost(m_context, &ServiceHostBase::RequestShutdown);
  }

}
