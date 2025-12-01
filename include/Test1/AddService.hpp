#ifndef SERVICE_FRAMEWORK_TEST1_ADDSERVICE_HPP
#define SERVICE_FRAMEWORK_TEST1_ADDSERVICE_HPP
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

#include <Test1/ServiceBase.hpp>
#include <Test1/ServiceConfig.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace Test1
{
  // Add Service - runs in its own thread
  class AddService : public ServiceBase
  {
  public:
    AddService()
      : ServiceBase("AddService")
    {
    }

    boost::asio::awaitable<double> AddAsync(const double a, const double b)
    {
      co_return co_await call(
        [=]()
        {
          spdlog::info("[AddService] {} + {}", a, b);
          std::this_thread::sleep_for(std::chrono::milliseconds(Config::ADD_SERVICE_DELAY_MS));
          return a + b;
        });
    }
  };

}

#endif