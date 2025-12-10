#ifndef SERVICE_FRAMEWORK_TEST3_EXAMPLESERVICEUSE_HPP
#define SERVICE_FRAMEWORK_TEST3_EXAMPLESERVICEUSE_HPP
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

#include <Test3/ServiceCallbackReceiver.hpp>
#include <Test3/ToFutureWithCallback.hpp>
#include <boost/asio/io_context.hpp>
#include <future>

namespace Test3
{
  /// @brief Example class demonstrating usage of ServiceCallbackReceiver and ToFutureWithCallback.
  ///
  /// This class shows how to:
  /// - Inherit from ServiceCallbackReceiver for automatic stop_token management
  /// - Use CallAsync helper for clean async operation invocation
  /// - Implement callback methods that receive std::future<T>
  ///
  /// The pattern ensures that callbacks are only invoked if the object is still alive,
  /// preventing use-after-free scenarios in async code.
  class ExampleServiceUse : public ServiceCallbackReceiver
  {
  public:
    /// @brief Constructs an example service user.
    /// @param executor The executor to run callbacks on.
    explicit ExampleServiceUse(boost::asio::any_io_executor executor)
      : ServiceCallbackReceiver(std::move(executor))
    {
    }

    /// @brief Callback invoked when async operation completes.
    /// @param future Future containing the result or exception from the async operation.
    void HandleResult(std::future<double> future)
    {
      try
      {
        double result = future.get();
        // Process result (e.g., update UI, log, etc.)
        // fmt::print("Result: {}\n", result);
      }
      catch (const std::exception& ex)
      {
        // Handle error
        // fmt::print("Error: {}\n", ex.what());
      }
    }

    /// @brief Example method that calls an async service operation.
    ///
    /// Demonstrates the clean syntax enabled by CallAsync:
    /// - No need to pass 'this' or executor explicitly
    /// - Just specify the callback method and the coroutine lambda
    /// - Returns a future that can be used for additional synchronization if needed
    void DoWork()
    {
      // Example: Get a service (pseudocode - replace with actual service resolution)
      // auto addService = GetService<IAddService>();

      // Call async operation with callback
      // Clean syntax: just method pointer and lambda
      auto future = CallAsync(&ExampleServiceUse::HandleResult,
                              []() -> boost::asio::awaitable<double>
                              {
                                // Example async operation
                                // return addService->AddAsync(1.0, 2.0);
                                co_return 42.0;    // Placeholder
                              });

      // Can optionally wait on future if synchronous result needed
      // double result = future.get();
    }
  };
}    // namespace Test3

#endif
