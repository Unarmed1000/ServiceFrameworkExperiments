#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_SERVICEDESCRIPTION_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_SERVICEDESCRIPTION_HPP
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

#include <string>

namespace Test2
{
  struct ServiceDescription
  {
    std::string Name;

    ServiceDescription() = default;
    ~ServiceDescription() = default;

    ServiceDescription(const ServiceDescription&) = delete;
    ServiceDescription& operator=(const ServiceDescription&) = delete;

    ServiceDescription(ServiceDescription&&) noexcept = default;
    ServiceDescription& operator=(ServiceDescription&&) noexcept = default;
  };

}

#endif