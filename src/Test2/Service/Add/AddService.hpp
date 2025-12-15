#ifndef SERVICE_FRAMEWORK_TEST2_SERVICE_ADD_ADDSERVICE_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICE_ADD_ADDSERVICE_HPP
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
#include <Test2/Services/Add/IAddService.hpp>
#include <spdlog/spdlog.h>

namespace Test2
{
  /// @brief Add Service implementation - runs in its own thread.
  class AddService final
    : public AsyncServiceBase
    , public IAddService
  {
  public:
    explicit AddService(const ServiceCreateInfo& createInfo);
    boost::asio::awaitable<double> AddAsync(const double a, const double b) final;
  };
}

#endif
