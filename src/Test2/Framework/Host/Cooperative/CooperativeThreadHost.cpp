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

#include <Test2/Framework/Exception/WrongThreadException.hpp>
#include <Test2/Framework/Host/Cooperative/CooperativeThreadHost.hpp>
#include <Test2/Framework/Host/IServiceHost.hpp>
#include <Test2/Framework/Host/ServiceHostProxy.hpp>
#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <fmt/std.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>
#include "CooperativeThreadServiceHost.hpp"

namespace Test2
{

  CooperativeThreadHost::CooperativeThreadHost(boost::asio::cancellation_slot cancel_slot)
    // Create the service host on the current thread
    : m_serviceHost(std::make_shared<CooperativeThreadServiceHost>())
    , m_sourceContext(ExecutorContext<ILifeTracker>(m_serviceHost, m_serviceHost->GetExecutor()))
    , m_targetContext(ExecutorContext<ServiceHostBase>(m_serviceHost, m_serviceHost->GetExecutor()))
    // Create the proxy for thread-safe access, but as this is a cooperative host on the same thread,
    // we can use the same dispatch context for source and target.
    , m_serviceHostProxy(std::make_shared<ServiceHostProxy>(DispatchContext(m_sourceContext, m_targetContext)))
  {
    // Register internal cancellation signal to stop the io_context
    m_cancellationSignal.slot().assign([serviceHost = m_serviceHost](boost::asio::cancellation_type) { serviceHost->RequestStop(); });

    // Register external cancellation slot to stop the io_context if provided
    if (cancel_slot.is_connected())
    {
      cancel_slot.assign([serviceHost = m_serviceHost](boost::asio::cancellation_type) { serviceHost->RequestStop(); });
    }
  }

  CooperativeThreadHost::~CooperativeThreadHost()
  {
    // Signal cancellation to stop the io_context
    m_cancellationSignal.emit(boost::asio::cancellation_type::terminal);
  }

  ExecutorContext<ILifeTracker> CooperativeThreadHost::GetExecutorContext() const
  {
    static bool warningLogged = false;
    if (!warningLogged)
    {
      spdlog::warn(
        "CooperativeThreadHost::GetExecutorContext() - Automatic wake on cross-thread post is NOT YET IMPLEMENTED. "
        "Posting to this executor will NOT trigger the wake callback.");
      warningLogged = true;
    }
    return m_sourceContext;
  }

  std::shared_ptr<IServiceHost> CooperativeThreadHost::GetServiceHost()
  {
    if (m_serviceHostProxy)
    {
      return m_serviceHostProxy;
    }
    throw std::runtime_error("Service host is no longer available");
  }

  ServiceProvider CooperativeThreadHost::GetServiceProvider()
  {
    if (!m_serviceHost)
    {
      throw std::runtime_error("Service host is no longer available");
    }

    // Verify thread access
    const auto currentThreadId = std::this_thread::get_id();
    if (currentThreadId != m_serviceHost->GetOwnerThreadId())
    {
      spdlog::error("CooperativeThreadHost accessed from wrong thread. Owner: {}, Caller: {}", m_serviceHost->GetOwnerThreadId(), currentThreadId);
      throw WrongThreadException("CooperativeThreadHost accessed from wrong thread");
    }

    return ServiceProvider(m_serviceHost->GetServiceProvider());
  }

  ProcessResult CooperativeThreadHost::Update()
  {
    if (!m_serviceHost)
    {
      throw std::runtime_error("Service host is no longer available");
    }
    return m_serviceHost->Update();
  }

  std::size_t CooperativeThreadHost::Poll()
  {
    if (!m_serviceHost)
    {
      throw std::runtime_error("Service host is no longer available");
    }
    return m_serviceHost->Poll();
  }
};
