#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_ASYNCSERVICEBASE_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_ASYNC_ASYNCSERVICEBASE_HPP
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

namespace Test2
{
  struct ServiceCreateInfo;

  /// @brief Base class for asynchronous services that run in their own thread.
  ///
  /// TODO: This class needs to be completed with:
  /// - Thread management (io_context, work_guard, thread)
  /// - A call() method to dispatch work to the service's thread
  /// - Proper lifecycle management (start/stop)
  /// - Integration with the service host for thread coordination
  class ASyncServiceBase : public IServiceControl
  {
  public:
    explicit ASyncServiceBase(const ServiceCreateInfo& /*creationInfo*/)
    {
      // TODO: Initialize io_context and work_guard here
      // TODO: Store any needed configuration from creationInfo
    }

    ~ASyncServiceBase() override = default;

  protected:
    // TODO: Add the following when implementing async functionality:
    //
    // boost::asio::io_context m_io_context;
    // boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work_guard;
    // std::thread m_thread;
    //
    // template <typename Func>
    // auto call(Func&& func) -> boost::asio::awaitable<decltype(std::declval<std::decay_t<Func>>()())>
    // {
    //   // Execute function on service thread via co_spawn
    // }
  };

}

#endif