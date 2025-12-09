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

#include "TestManagedThreadLifecycle.hpp"
#include <spdlog/spdlog.h>

namespace Test2
{
  TestManagedThreadLifecycle::TestManagedThreadLifecycle(ManagedThreadHost& host)
    : m_host(host)
  {
  }

  boost::asio::awaitable<void> TestManagedThreadLifecycle::StartAsync()
  {
    co_await m_host.StartAsync();
  }

  boost::asio::awaitable<void> TestManagedThreadLifecycle::StartServicesAsync(std::vector<StartServiceRecord> services,
                                                                              ServiceLaunchPriority priority)
  {
    // Start services on the host
    co_await m_host.GetServiceHost()->TryStartServicesAsync(std::move(services), priority);

    // Track the priority for automatic cleanup
    m_startedPriorities.push_back(priority);
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>> TestManagedThreadLifecycle::ShutdownServicesAsync(ServiceLaunchPriority priority)
  {
    // Shutdown services at this priority
    auto errors = co_await m_host.GetServiceHost()->TryShutdownServicesAsync(priority);

    // Track that this priority has been shut down
    m_shutdownPriorities.insert(priority);

    co_return errors;
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>> TestManagedThreadLifecycle::ShutdownAllServicesAsync()
  {
    std::vector<std::exception_ptr> allErrors;

    // Shutdown in reverse order of startup (lowest priority first, then higher)
    for (auto it = m_startedPriorities.rbegin(); it != m_startedPriorities.rend(); ++it)
    {
      // Skip priorities that have already been shut down
      if (m_shutdownPriorities.find(*it) != m_shutdownPriorities.end())
      {
        continue;
      }

      try
      {
        auto errors = co_await m_host.GetServiceHost()->TryShutdownServicesAsync(*it);
        allErrors.insert(allErrors.end(), errors.begin(), errors.end());
        m_shutdownPriorities.insert(*it);
      }
      catch (...)
      {
        allErrors.push_back(std::current_exception());
      }
    }

    // Clear the tracking
    m_startedPriorities.clear();
    m_shutdownPriorities.clear();

    co_return allErrors;
  }

  boost::asio::awaitable<std::vector<std::exception_ptr>> TestManagedThreadLifecycle::TryShutdownAsync()
  {
    // First shutdown all services
    auto errors = co_await ShutdownAllServicesAsync();

    // Then shutdown the managed thread
    try
    {
      co_await m_host.TryShutdownAsync();
    }
    catch (...)
    {
      errors.push_back(std::current_exception());
    }

    co_return errors;
  }

  ManagedThreadHost& TestManagedThreadLifecycle::GetHost() noexcept
  {
    return m_host;
  }

  const ManagedThreadHost& TestManagedThreadLifecycle::GetHost() const noexcept
  {
    return m_host;
  }
}
