#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGEDTHREADSERVICEHOST_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_MANAGEDTHREADSERVICEHOST_HPP
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

#include <Common/AggregateException.hpp>
#include <Test2/Framework/Exception/InvalidServiceFactoryException.hpp>
#include <Test2/Framework/Host/ManagedThreadServiceProvider.hpp>
#include <Test2/Framework/Host/ServiceInstanceInfo.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <Test2/Framework/Provider/ServiceProviderProxy.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <vector>

namespace Test2
{
  /// @brief Service host that lives on a managed thread. All methods are called on the managed thread.
  class ManagedThreadServiceHost
  {
    std::unique_ptr<boost::asio::io_context> m_ioContext;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work;

    std::shared_ptr<ManagedThreadServiceProvider> m_provider;

  public:
    ManagedThreadServiceHost()
      : m_ioContext(std::make_unique<boost::asio::io_context>())
      , m_work(boost::asio::make_work_guard(*m_ioContext))
      , m_provider(std::make_shared<ManagedThreadServiceProvider>())
    {
      spdlog::info("Created at {}", static_cast<void*>(this));
    }

    ~ManagedThreadServiceHost()
    {
      spdlog::info("Destroying at {}", static_cast<void*>(this));
      // Called on the managed thread during shutdown
      m_work.reset();
    }

    ManagedThreadServiceHost(const ManagedThreadServiceHost&) = delete;
    ManagedThreadServiceHost& operator=(const ManagedThreadServiceHost&) = delete;
    ManagedThreadServiceHost(ManagedThreadServiceHost&&) = delete;
    ManagedThreadServiceHost& operator=(ManagedThreadServiceHost&&) = delete;

    /// @brief Starts and runs the io_context event loop. Called on the managed thread.
    /// @param cancel_slot Cancellation slot to stop the io_context run loop.
    /// @return An awaitable that completes when the io_context run loop exits.
    boost::asio::awaitable<void> RunAsync(boost::asio::cancellation_slot cancel_slot = {})
    {
      if (cancel_slot.is_connected())
      {
        cancel_slot.assign(
          [this](boost::asio::cancellation_type)
          {
            m_work.reset();
            m_ioContext->stop();
          });
      }

      m_ioContext->run();
      co_return;
    }

    boost::asio::io_context& GetIoContext()
    {
      return *m_ioContext;
    }

    const boost::asio::io_context& GetIoContext() const
    {
      return *m_ioContext;
    }

    boost::asio::awaitable<void> TryStartServicesAsync(std::vector<StartServiceRecord> services, ServiceLaunchPriority currentPriority)
    {
      co_await boost::asio::co_spawn(
        *m_ioContext, [this, services = std::move(services), currentPriority]() mutable -> boost::asio::awaitable<void>
        { co_await DoTryStartServicesAsync(std::move(services), currentPriority); }, boost::asio::use_awaitable);
      co_return;
    }

  private:
    boost::asio::awaitable<void> DoTryStartServicesAsync(std::vector<StartServiceRecord> services, ServiceLaunchPriority currentPriority)
    {
      // Handle empty service list
      if (services.empty())
      {
        spdlog::warn("TryStartServicesAsync called with empty service list at priority {}", currentPriority.GetValue());
        co_return;
      }

      // Validate service factories
      ValidateServiceFactories(services);

      // Create proxy for provider - can be cleared on failure
      auto providerProxy = std::make_shared<ServiceProviderProxy>(m_provider);
      std::weak_ptr<IServiceProvider> providerWeak = providerProxy;
      ServiceProvider serviceProvider(providerWeak);
      ServiceCreateInfo createInfo(serviceProvider);


      std::vector<ServiceInitRecord> initRecords;

      try
      {
        // Phase 1: Create all service instances
        CreateServiceInstances(services, createInfo, initRecords);

        // Phase 2: Initialize all services concurrently
        co_await InitializeServices(initRecords, createInfo);

        // Phase 3: Handle failures with rollback or register successful services
        co_await ProcessInitializationResults(initRecords, currentPriority, providerProxy);
      }
      catch (...)
      {
        // Clear the proxy on any exception
        providerProxy->Clear();
        throw;
      }

      co_return;
    }

    struct ServiceInitRecord
    {
      std::string ServiceName;
      std::shared_ptr<IServiceControl> Service;
      ServiceInstanceInfo InstanceInfo;
      std::exception_ptr InitException;
      bool InitSucceeded = false;
    };

    void ValidateServiceFactories(const std::vector<StartServiceRecord>& services)
    {
      for (const auto& serviceRecord : services)
      {
        if (!serviceRecord.Factory)
        {
          throw InvalidServiceFactoryException(serviceRecord.ServiceName);
        }
      }
    }

    void CreateServiceInstances(std::vector<StartServiceRecord>& services, const ServiceCreateInfo& createInfo,
                                std::vector<ServiceInitRecord>& initRecords)
    {
      initRecords.reserve(services.size());

      for (auto& serviceRecord : services)
      {
        ServiceInitRecord record;
        record.ServiceName = serviceRecord.ServiceName;

        spdlog::info("Creating service: {}", serviceRecord.ServiceName);

        // Get supported interfaces from factory
        auto supportedInterfaces = serviceRecord.Factory->GetSupportedInterfaces();
        if (supportedInterfaces.empty())
        {
          throw std::invalid_argument(fmt::format("Factory for service '{}' reports no supported interfaces", serviceRecord.ServiceName));
        }

        // Create service instance using first supported interface
        record.Service = serviceRecord.Factory->Create(supportedInterfaces[0], createInfo);
        if (!record.Service)
        {
          throw std::runtime_error(fmt::format("Factory for service '{}' returned null service", serviceRecord.ServiceName));
        }

        // Prepare InstanceInfo
        record.InstanceInfo.Service = record.Service;
        record.InstanceInfo.SupportedInterfaces.reserve(supportedInterfaces.size());
        for (const auto& typeInfo : supportedInterfaces)
        {
          record.InstanceInfo.SupportedInterfaces.push_back(&typeInfo);
        }

        initRecords.push_back(std::move(record));
      }
    }

    boost::asio::awaitable<void> InitializeServices(std::vector<ServiceInitRecord>& initRecords, const ServiceCreateInfo& createInfo)
    {
      for (auto& record : initRecords)
      {
        try
        {
          spdlog::info("Initializing service: {}", record.ServiceName);

          auto initResult = co_await record.Service->InitAsync(createInfo);
          if (initResult != ServiceInitResult::Success)
          {
            throw std::runtime_error("Service '" + record.ServiceName +
                                     "' initialization failed with result: " + std::to_string(static_cast<int>(initResult)));
          }

          record.InitSucceeded = true;
          spdlog::info("Service initialized successfully: {}", record.ServiceName);
        }
        catch (...)
        {
          record.InitException = std::current_exception();
          spdlog::error("Service initialization failed: {}", record.ServiceName);
        }
      }

      co_return;
    }


    boost::asio::awaitable<void> ProcessInitializationResults(std::vector<ServiceInitRecord>& initRecords, ServiceLaunchPriority currentPriority,
                                                              std::shared_ptr<ServiceProviderProxy> providerProxy)
    {
      // Collect failures and successful services
      std::vector<std::exception_ptr> initFailures;
      std::vector<std::shared_ptr<IServiceControl>> successfulServices;

      for (const auto& record : initRecords)
      {
        if (record.InitException)
        {
          initFailures.push_back(record.InitException);
        }
        else if (record.InitSucceeded)
        {
          successfulServices.push_back(record.Service);
        }
      }

      // If any initializations failed, perform rollback
      if (!initFailures.empty())
      {
        auto shutdownFailures = co_await RollbackServices(successfulServices);

        // Clear the proxy to prevent further service access
        providerProxy->Clear();

        // Combine initialization and shutdown failures
        std::vector<std::exception_ptr> allFailures = std::move(initFailures);
        allFailures.insert(allFailures.end(), shutdownFailures.begin(), shutdownFailures.end());

        // Throw aggregate exception with all failures
        throw Common::AggregateException("Service initialization failed", std::move(allFailures));
      }

      // All services initialized successfully - register with provider
      RegisterServicesWithProvider(initRecords, currentPriority);
    }

    boost::asio::awaitable<std::vector<std::exception_ptr>> RollbackServices(const std::vector<std::shared_ptr<IServiceControl>>& successfulServices)
    {
      spdlog::warn("Performing rollback of {} successful services", successfulServices.size());

      std::vector<std::exception_ptr> shutdownFailures;

      // Shutdown in reverse order
      for (auto it = successfulServices.rbegin(); it != successfulServices.rend(); ++it)
      {
        try
        {
          auto shutdownResult = co_await (*it)->ShutdownAsync();
          if (shutdownResult != ServiceShutdownResult::Success)
          {
            spdlog::warn("Service shutdown during rollback returned non-success result: {}", static_cast<int>(shutdownResult));
          }
        }
        catch (...)
        {
          shutdownFailures.push_back(std::current_exception());
          spdlog::error("Exception during service shutdown in rollback");
        }
      }

      co_return shutdownFailures;
    }

    void RegisterServicesWithProvider(std::vector<ServiceInitRecord>& initRecords, ServiceLaunchPriority currentPriority)
    {
      std::vector<ServiceInstanceInfo> serviceInfos;
      serviceInfos.reserve(initRecords.size());

      for (auto& record : initRecords)
      {
        serviceInfos.push_back(std::move(record.InstanceInfo));
      }

      m_provider->RegisterPriorityGroup(currentPriority, std::move(serviceInfos));

      spdlog::info("Successfully initialized and registered {} services at priority {}", initRecords.size(), currentPriority.GetValue());
    }

  protected:
    // Execute function on service thread - handles both copyable and move-only types
    template <typename Func>
    auto call(Func&& func) -> boost::asio::awaitable<decltype(std::declval<std::decay_t<Func>>()())>
    {
      using ResultType = decltype(std::declval<std::decay_t<Func>>()());

      // Use co_spawn to execute on service thread
      co_return co_await boost::asio::co_spawn(
        *m_ioContext,
        [func = std::forward<Func>(func)]() mutable -> boost::asio::awaitable<ResultType>
        {
          if constexpr (std::is_void_v<ResultType>)
          {
            func();
            co_return;
          }
          else
          {
            co_return func();
          }
        },
        boost::asio::use_awaitable);
    }
  };
}

#endif