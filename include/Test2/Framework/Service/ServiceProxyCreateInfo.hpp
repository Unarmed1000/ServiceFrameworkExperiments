#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ServicePROXYCreateInfo_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ServicePROXYCreateInfo_HPP
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

#include <Test2/Framework/Lifecycle/DispatchContext.hpp>
#include <Test2/Framework/Lifecycle/ILifeTracker.hpp>
#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <Test2/Framework/Service/IService.hpp>

namespace Test2
{
  struct ServiceProxyCreateInfo
  {
  private:
    DispatchContext<ILifeTracker, IService> m_dispatchContext;

  public:
    ServiceProvider Provider;

    explicit ServiceProxyCreateInfo(DispatchContext<ILifeTracker, IService> dispatchContext, ServiceProvider provider)
      : m_dispatchContext(std::move(dispatchContext))
      , Provider(std::move(provider))
    {
    }

    template <typename TServiceClass>
    [[nodiscard]] DispatchContext<ILifeTracker, TServiceClass> GetDispatchContext() const
    {
      auto targetPtr = std::dynamic_pointer_cast<TServiceClass>(m_dispatchContext.GetTargetContext().TryLock());
      if (targetPtr == nullptr)
      {
        throw std::runtime_error("ServiceProxyCreateInfo::GetDispatchContext: Target service has expired or is of incorrect type");
      }
      return DispatchContext<ILifeTracker, TServiceClass>(
        m_dispatchContext.GetSourceContext(), ExecutorContext<TServiceClass>(targetPtr, m_dispatchContext.GetTargetContext().GetExecutor()));
    }
  };
}

#endif