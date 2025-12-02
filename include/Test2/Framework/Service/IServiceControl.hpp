#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ISERVICECONTROL_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ISERVICECONTROL_HPP
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

#include <Test2/Framework/Service/IService.hpp>
#include <Test2/Framework/Service/ServiceInitResult.hpp>
#include <Test2/Framework/Service/ServiceProcessResult.hpp>
#include <Test2/Framework/Service/ServiceShutdownResult.hpp>
#include <boost/asio/awaitable.hpp>

namespace Test2
{
  struct ServiceCreateInfo;

  class IServiceControl : public IService
  {
  public:
    virtual ~IServiceControl() = default;

    virtual boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& creationInfo) = 0;
    virtual boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync() = 0;

    virtual ServiceProcessResult Process() = 0;
  };

}

#endif