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

#include "SubtractService.hpp"
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <Test2/Services/ServiceConfig.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace Test2
{
  SubtractService::SubtractService(const ServiceCreateInfo& createInfo)
    : AsyncServiceBase(createInfo)
  {
    spdlog::debug("SubtractService: constructed");
  }

  boost::asio::awaitable<double> SubtractService::SubtractAsync(const double a, const double b)
  {
    spdlog::info("[SubtractService] {} - {}", a, b);
    std::this_thread::sleep_for(std::chrono::milliseconds(Config::SUBTRACT_SERVICE_DELAY_MS));
    co_return a - b;
  }
}
