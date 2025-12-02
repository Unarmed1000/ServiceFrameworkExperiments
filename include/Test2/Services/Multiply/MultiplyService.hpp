#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_MULTIPLY_MULTIPLYSERVICE_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_MULTIPLY_MULTIPLYSERVICE_HPP
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
#include <Test2/Services/Multiply/IMultiplyService.hpp>
#include <Test2/Services/ServiceConfig.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace Test2
{
  /// @brief Multiply Service implementation - runs in its own thread.
  class MultiplyService final
    : public ASyncServiceBase
    , public IMultiplyService
  {
  public:
    explicit MultiplyService(const ServiceCreateInfo& createInfo)
      : ASyncServiceBase(createInfo)
    {
      spdlog::debug("MultiplyService: constructed");
    }

    ~MultiplyService() override = default;

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*createInfo*/) override
    {
      spdlog::info("MultiplyService: InitAsync");
      co_return ServiceInitResult{};
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() override
    {
      spdlog::info("MultiplyService: ShutdownAsync");
      co_return ServiceShutdownResult{};
    }

    ProcessResult Process() override
    {
      return ProcessResult{ProcessStatus::Idle};
    }

    boost::asio::awaitable<double> MultiplyAsync(const double a, const double b) override
    {
      spdlog::info("[MultiplyService] {} * {}", a, b);
      std::this_thread::sleep_for(std::chrono::milliseconds(Config::MULTIPLY_SERVICE_DELAY_MS));
      co_return a* b;
    }
  };

}

#endif
