#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_SERVICEINSTANCEINFO_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_SERVICEINSTANCEINFO_HPP
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

#include <Test2/Framework/Service/IServiceControl.hpp>
#include <memory>
#include <typeinfo>
#include <vector>

namespace Test2
{
  /// @brief Information about a registered service instance.
  ///
  /// Stores the service control interface and the list of service interfaces
  /// that this instance supports for type-based lookup.
  struct ServiceInstanceInfo
  {
    std::shared_ptr<IServiceControl> Service;
    std::vector<const std::type_info*> SupportedInterfaces;
  };
}

#endif
