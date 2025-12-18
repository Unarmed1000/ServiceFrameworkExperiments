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

#include <Test2/Services/Subtract/ISubtractService.hpp>
#include <Test2/Services/Subtract/SubtractAsyncServiceFactory.hpp>
#include <memory>
#include <stdexcept>
#include "SubtractService.hpp"
#include "SubtractServiceProxy.hpp"

namespace Test2
{
  SubtractAsyncServiceFactory::SubtractAsyncServiceFactory()
    : AsyncServiceFactory(typeid(ISubtractService))
  {
  }

  std::shared_ptr<IServiceProxyControl> SubtractAsyncServiceFactory::CreateProxy(const ServiceProxyCreateInfo& createInfo)
  {
    return std::make_shared<SubtractServiceProxy>(createInfo);
  }

  std::shared_ptr<IServiceControl> SubtractAsyncServiceFactory::Create(const ServiceCreateInfo& createInfo)
  {
    return std::make_shared<SubtractService>(createInfo);
  }
}
