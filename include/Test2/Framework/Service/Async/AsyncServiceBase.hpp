#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_ASYNCSERVICEBASE_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_ASYNCSERVICEBASE_HPP
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

#include <Test2/Framework/Lifecycle/ILifeTracker.hpp>
#include <Test2/Framework/Service/IServiceControl.hpp>
#include <spdlog/spdlog.h>
namespace Test2
{
  struct ServiceCreateInfo;

  /// @brief Base class for asynchronous services that run in their own thread.
  class AsyncServiceBase
    : public IServiceControl
    , public ILifeTracker
  {
  public:
    explicit AsyncServiceBase(const ServiceCreateInfo& /*serviceCreationInfo*/)
    {
    }

    ~AsyncServiceBase() override = default;


    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*serviceCreationInfo*/) override
    {
      spdlog::info("AsyncServiceBase: InitAsync");
      co_return ServiceInitResult{};
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override
    {
      spdlog::info("AsyncServiceBase: ShutdownAsync");
      co_return ServiceShutdownResult{};
    }

    ProcessResult Process() override
    {
      return ProcessResult::NoSleepLimit();
    }
  };

}

#endif