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

#include <Test2/Services/Multiply/MultiplyAsyncServiceFactory.hpp>
#include "MultiplyService.hpp"
#include "MultiplyServiceProxy.hpp"

namespace Test2
{
  MultiplyAsyncServiceFactory::MultiplyAsyncServiceFactory()
    : AsyncServiceFactory(typeid(IMultiplyService))
  {
  }

  std::shared_ptr<IServiceProxyControl> MultiplyAsyncServiceFactory::CreateProxy(const ServiceProxyCreateInfo& createInfo)
  {
    return std::make_shared<MultiplyServiceProxy>(createInfo);
  }

  std::shared_ptr<IServiceControl> MultiplyAsyncServiceFactory::Create(const ServiceCreateInfo& createInfo)
  {
    return std::make_shared<MultiplyService>(createInfo);
  }

}
