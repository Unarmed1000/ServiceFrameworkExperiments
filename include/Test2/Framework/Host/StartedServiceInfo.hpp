#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_STARTEDSERVICEINFO_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_STARTEDSERVICEINFO_HPP
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
#include <Test2/Framework/Service/IService.hpp>

namespace Test2
{
  /// @brief Information about a successfully started service.
  ///
  /// Contains the executor context for safe cross-thread invocation of the service.
  /// Designed to be extensible - additional fields can be added in the future without breaking API.
  struct StartedServiceInfo
  {
    /// @brief Executor context for the started service, enabling cross-thread invocation with lifetime tracking.
    ExecutorContext<IService> ServiceExecutor;

    StartedServiceInfo() = default;
    ~StartedServiceInfo() = default;
    StartedServiceInfo(const StartedServiceInfo&) = default;
    StartedServiceInfo& operator=(const StartedServiceInfo&) = default;
    StartedServiceInfo(StartedServiceInfo&&) = default;
    StartedServiceInfo& operator=(StartedServiceInfo&&) = default;

    explicit StartedServiceInfo(ExecutorContext<IService> serviceExecutor)
      : ServiceExecutor(std::move(serviceExecutor))
    {
    }
  };
}

#endif
