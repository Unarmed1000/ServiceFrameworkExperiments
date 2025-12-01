#ifndef SERVICE_FRAMEWORK_TEST1_COMPLEXSERVICE_HPP
#define SERVICE_FRAMEWORK_TEST1_COMPLEXSERVICE_HPP
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

#include <Common/ComplexData.hpp>
#include <Test1/ServiceBase.hpp>
#include <Test1/ServiceConfig.hpp>
#include <spdlog/spdlog.h>
#include <exception>

namespace Test1
{
  // Subtract Service - runs in its own thread
  class ComplexService : public ServiceBase
  {
  public:
    class TestException : public std::runtime_error
    {
    public:
      explicit TestException(const char* const psz)
        : std::runtime_error(psz)
      {
      }
    };

    ComplexService()
      : ServiceBase("ComplexService")
    {
    }

    boost::asio::awaitable<std::string> StuffAsync(std::unique_ptr<Common::ComplexData> data)
    {
      co_return co_await call(
        [data = std::move(data)]() mutable
        {
          if (!data)
            return std::string{"invalid_data"};
          spdlog::info("[ComplexService] Processing complex data");
          std::this_thread::sleep_for(std::chrono::milliseconds(Config::COMPLEX_SERVICE_DELAY_MS));
          return data->Name + "_processed";
        });
    }

    boost::asio::awaitable<std::string> StuffWithExceptionAsync(std::unique_ptr<Common::ComplexData> data)
    {
      co_return co_await call(
        [data = std::move(data)]() mutable
        {
          if (!data)
            return std::string{"invalid_data"};
          spdlog::info("[ComplexService] StuffWithExceptionAsync");
          std::this_thread::sleep_for(std::chrono::milliseconds(Config::COMPLEX_SERVICE_DELAY_MS));

          throw TestException("Forced exception");
        });
    }
  };
}


#endif