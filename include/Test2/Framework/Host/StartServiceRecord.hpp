#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_StartServiceRecord_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_StartServiceRecord_HPP
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

#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <memory>
#include <string>

namespace Test2
{
  struct StartServiceRecord
  {
    std::string ServiceName;

    /// @brief The service factory that creates service instances.
    /// Ownership is held by this record.
    std::unique_ptr<IServiceFactory> Factory;

    StartServiceRecord(std::string serviceName, std::unique_ptr<IServiceFactory> factory)
      : ServiceName(std::move(serviceName))
      , Factory(std::move(factory))
    {
    }

    StartServiceRecord(const StartServiceRecord&) = delete;
    StartServiceRecord& operator=(const StartServiceRecord&) = delete;

    StartServiceRecord(StartServiceRecord&&) = default;
    StartServiceRecord& operator=(StartServiceRecord&&) = default;
  };
}

#endif
