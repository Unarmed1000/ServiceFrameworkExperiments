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
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <stdexcept>
#include "ServiceHostBase.hpp"

namespace Test2
{
  namespace
  {
    boost::asio::awaitable<void> DoTryStartServicesAsync(std::weak_ptr<ServiceHostBase> service, std::vector<StartServiceRecord> services,
                                                         const ServiceLaunchPriority currentPriority)
    {
      auto servicePtr = service.lock();
      if (!servicePtr)
      {
        throw std::runtime_error("ServiceHostProxy: service host has been destroyed");
      }
      co_await servicePtr->TryStartServicesAsync(std::move(services), currentPriority);
    }

    boost::asio::awaitable<std::vector<std::exception_ptr>> DoTryShutdownServicesAsync(std::weak_ptr<ServiceHostBase> service,
                                                                                       const ServiceLaunchPriority priority)
    {
      auto servicePtr = service.lock();
      if (!servicePtr)
      {
        throw std::runtime_error("ServiceHostProxy: service host has been destroyed");
      }
      co_return co_await servicePtr->TryShutdownServicesAsync(priority);
    }

  }

  ServiceHostProxy::ServiceHostProxy(std::weak_ptr<ServiceHostBase> service, boost::asio::any_io_executor executor)
    : m_service(std::move(service))
    , m_executor(std::move(executor))
  {
  }

  ServiceHostProxy::~ServiceHostProxy() = default;


  boost::asio::awaitable<void> ServiceHostProxy::TryStartServicesAsync(std::vector<StartServiceRecord> services,
                                                                       const ServiceLaunchPriority currentPriority)
  {
    co_await boost::asio::co_spawn(m_executor, DoTryStartServicesAsync(std::move(m_service), std::move(services), currentPriority),
                                   boost::asio::use_awaitable);
    co_return;
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>> ServiceHostProxy::TryShutdownServicesAsync(const ServiceLaunchPriority priority)
  {
    co_return co_await boost::asio::co_spawn(m_executor, DoTryShutdownServicesAsync(m_service, priority), boost::asio::use_awaitable);
  }
}
