#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_COOPERATIVE_COOPERATIVETHREADHOST_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_COOPERATIVE_COOPERATIVETHREADHOST_HPP
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

#include <Test2/Framework/Lifecycle/ExecutorContext.hpp>
#include <Test2/Framework/Lifecycle/ILifeTracker.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <memory>

namespace Test2
{
  class IThreadSafeServiceHost;
  class CooperativeThreadServiceHost;
  class ServiceHostBase;
  struct ProcessResult;

  /// @brief Manages a cooperative service host that runs on the current thread.
  class CooperativeThreadHost
  {
    std::shared_ptr<CooperativeThreadServiceHost> m_serviceHost;
    ExecutorContext<ILifeTracker> m_sourceContext;
    ExecutorContext<ServiceHostBase> m_targetContext;

    std::shared_ptr<IThreadSafeServiceHost> m_serviceHostProxy;
    boost::asio::cancellation_signal m_cancellationSignal;

  public:
    CooperativeThreadHost(const CooperativeThreadHost&) = delete;
    CooperativeThreadHost& operator=(const CooperativeThreadHost&) = delete;
    CooperativeThreadHost(CooperativeThreadHost&&) = delete;
    CooperativeThreadHost& operator=(CooperativeThreadHost&&) = delete;

    /// @brief Constructs a cooperative service host on the current thread.
    /// @param cancel_slot Optional cancellation slot to stop the host.
    explicit CooperativeThreadHost(boost::asio::cancellation_slot cancel_slot = {});
    ~CooperativeThreadHost();

    ExecutorContext<ILifeTracker> GetExecutorContext() const
    {
      return m_sourceContext;
    }

    std::shared_ptr<IThreadSafeServiceHost> GetServiceHost();

    /// @brief Polls the io_context and processes all services.
    ///
    /// This is the primary method to call from your main loop. It:
    /// 1. Calls Poll() to process any ready async handlers
    /// 2. Calls ProcessServices() to run service Process() methods
    /// 3. Returns the aggregated ProcessResult with sleep hints
    ///
    /// @return Aggregated ProcessResult from all services.
    ProcessResult Update();

    /// @brief Process all ready handlers without blocking.
    ///
    /// @return The number of handlers that were executed.
    std::size_t Poll();
  };
}

#endif