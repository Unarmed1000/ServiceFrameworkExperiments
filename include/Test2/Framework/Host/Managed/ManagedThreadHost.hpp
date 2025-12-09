#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGED_MANAGEDTHREADHOST_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGED_MANAGEDTHREADHOST_HPP
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

#include <Test2/Framework/Host/IThreadSafeServiceHost.hpp>
#include <Test2/Framework/Host/Managed/ManagedThreadRecord.hpp>
#include <Test2/Framework/Lifecycle/ExecutorContext.hpp>
#include <Test2/Framework/Lifecycle/ILifeTracker.hpp>
#include <memory>
#include <thread>

namespace Test2
{
  class ServiceHostProxy;
  class ILifeTracker;

  /// @brief Manages a thread that runs a ManagedThreadServiceHost.
  class ManagedThreadHost
  {
    ExecutorContext<ILifeTracker> m_sourceContext;
    std::shared_ptr<ServiceHostProxy> m_serviceHostProxy;
    std::thread m_thread;

  public:
    ManagedThreadHost(ExecutorContext<ILifeTracker> sourceContext);
    ~ManagedThreadHost();
    ManagedThreadHost(const ManagedThreadHost&) = delete;
    ManagedThreadHost& operator=(const ManagedThreadHost&) = delete;
    ManagedThreadHost(ManagedThreadHost&&) = delete;
    ManagedThreadHost& operator=(ManagedThreadHost&&) = delete;


    ExecutorContext<ILifeTracker> GetExecutorContext() const
    {
      return m_sourceContext;
    }

    /// @brief Starts the managed thread.
    /// @param cancel_slot Cancellation slot to stop the thread.
    /// @return An awaitable that completes when the thread has started, containing a ManagedThreadRecord with the lifetime awaitable.
    boost::asio::awaitable<ManagedThreadRecord> StartAsync();
    boost::asio::awaitable<bool> TryShutdownAsync();

    std::shared_ptr<IThreadSafeServiceHost> GetServiceHost();
  };
}

#endif