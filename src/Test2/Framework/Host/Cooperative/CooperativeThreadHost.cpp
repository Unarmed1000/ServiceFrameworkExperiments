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

#include <Test2/Framework/Host/Cooperative/CooperativeThreadHost.hpp>
#include <Test2/Framework/Host/Cooperative/CooperativeThreadServiceHost.hpp>
#include <Test2/Framework/Host/IThreadSafeServiceHost.hpp>
#include <Test2/Framework/Host/ServiceHostProxy.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <stdexcept>

namespace Test2
{

  CooperativeThreadHost::CooperativeThreadHost(boost::asio::cancellation_slot cancel_slot)
  {
    // Create the service host on the current thread
    m_serviceHost = std::make_shared<CooperativeThreadServiceHost>();
    m_serviceHostProxy = std::make_shared<ServiceHostProxy>(m_serviceHost);

    // Register internal cancellation signal to stop the io_context
    m_cancellationSignal.slot().assign([serviceHost = m_serviceHost](boost::asio::cancellation_type) { serviceHost->GetIoContext().stop(); });

    // Register external cancellation slot to stop the io_context if provided
    if (cancel_slot.is_connected())
    {
      cancel_slot.assign([serviceHost = m_serviceHost](boost::asio::cancellation_type) { serviceHost->GetIoContext().stop(); });
    }
  }

  CooperativeThreadHost::~CooperativeThreadHost()
  {
    // Signal cancellation to stop the io_context
    m_cancellationSignal.emit(boost::asio::cancellation_type::terminal);
  }

  std::shared_ptr<IThreadSafeServiceHost> CooperativeThreadHost::GetServiceHost()
  {
    if (m_serviceHostProxy)
    {
      return m_serviceHostProxy;
    }
    throw std::runtime_error("Service host is no longer available");
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
