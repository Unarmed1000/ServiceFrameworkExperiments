#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_DIVIDE_DIVIDESERVICE_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_DIVIDE_DIVIDESERVICE_HPP
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

#include <Test2/Framework/Service/Async/AsyncServiceBase.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <Test2/Framework/Service/ServiceInitResult.hpp>
#include <Test2/Framework/Service/ServiceShutdownResult.hpp>
#include <Test2/Services/Divide/IDivideService.hpp>
#include <Test2/Services/ServiceConfig.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace Test2
{
  /// @brief Divide Service implementation - runs in its own thread.
  class DivideService final
    : public ASyncServiceBase
    , public IDivideService
  {
  public:
    explicit DivideService(const ServiceCreateInfo& createInfo)
      : ASyncServiceBase(createInfo)
    {
      spdlog::debug("DivideService: constructed");
    }

    ~DivideService() override = default;

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*createInfo*/) override
    {
      spdlog::info("DivideService: InitAsync");
      co_return ServiceInitResult{};
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override
    {
      spdlog::info("DivideService: ShutdownAsync");
      co_return ServiceShutdownResult{};
    }

    ProcessResult Process() override
    {
      return ProcessResult{ProcessStatus::Idle};
    }

    boost::asio::awaitable<double> DivideAsync(const double a, const double b) override
    {
      if (b == 0.0)
      {
        spdlog::error("[DivideService] Division by zero: {} / {}", a, b);
        throw std::runtime_error("Division by zero");
      }
      spdlog::info("[DivideService] {} / {}", a, b);
      std::this_thread::sleep_for(std::chrono::milliseconds(Config::DIVIDE_SERVICE_DELAY_MS));
      co_return a / b;
    }
  };

}

#endif
