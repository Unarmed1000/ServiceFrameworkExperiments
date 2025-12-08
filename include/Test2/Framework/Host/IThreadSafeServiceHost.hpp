#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_ITHREADSAFESERVICEHOST_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_ITHREADSAFESERVICEHOST_HPP
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

#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <boost/asio/awaitable.hpp>
#include <exception>
#include <vector>

namespace Test2
{
  class IThreadSafeServiceHost
  {
  public:
    IThreadSafeServiceHost() = default;
    virtual ~IThreadSafeServiceHost() {};

    IThreadSafeServiceHost(const IThreadSafeServiceHost&) = delete;
    IThreadSafeServiceHost& operator=(const IThreadSafeServiceHost&) = delete;
    IThreadSafeServiceHost(IThreadSafeServiceHost&&) = delete;
    IThreadSafeServiceHost& operator=(IThreadSafeServiceHost&&) = delete;

    /// @brief Try to start services at a given priority level.
    ///
    /// This method can be called from any thread. The work is marshalled onto the
    /// service thread via co_spawn, ensuring thread-safe access to internal state.
    ///
    /// Services are created, initialized, and registered with the provider.
    /// On failure, successfully initialized services are rolled back.
    ///
    /// @param services Services to start.
    /// @param currentPriority Priority level for this group.
    /// @return Awaitable that completes when services are started.
    virtual boost::asio::awaitable<void> TryStartServicesAsync(std::vector<StartServiceRecord> services,
                                                               const ServiceLaunchPriority currentPriority) = 0;

    /// @brief Shutdown services at a specific priority level.
    ///
    /// This method can be called from any thread. The work is marshalled onto the
    /// service thread via co_spawn, ensuring thread-safe access to internal state.
    ///
    /// @param priority The priority level to shut down.
    /// @return Awaitable containing any exceptions that occurred during shutdown.
    virtual boost::asio::awaitable<std::vector<std::exception_ptr>> TryShutdownServicesAsync(const ServiceLaunchPriority priority) = 0;
  };
}

#endif
