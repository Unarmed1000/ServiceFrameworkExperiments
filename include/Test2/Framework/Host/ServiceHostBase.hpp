#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_SERVICEHOSTBASE_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_HOST_SERVICEHOSTBASE_HPP
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
#include <Test2/Framework/Host/Managed/ManagedThreadServiceProvider.hpp>
#include <Test2/Framework/Host/ServiceInstanceInfo.hpp>
#include <Test2/Framework/Host/StartServiceRecord.hpp>
#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <Test2/Framework/Provider/ServiceProviderProxy.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Service/IServiceControl.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <vector>

namespace Test2
{
  /// @brief Base class for service hosts providing shared service management logic.
  ///
  /// This base class contains thread-agnostic service lifecycle management including:
  /// - Service validation, creation, and initialization
  /// - Service registration with the provider
  /// - Rollback on initialization failure
  /// - Processing services and aggregating results
  ///
  /// Derived classes must implement GetIoContext() to provide the execution context.
  class ServiceHostBase
  {
  protected:
    std::shared_ptr<ManagedThreadServiceProvider> m_provider;

    /// @brief Record tracking service initialization state.
    struct ServiceInitRecord
    {
      std::string ServiceName;
      std::shared_ptr<IServiceControl> Service;
      ServiceInstanceInfo InstanceInfo;
      std::exception_ptr InitException;
      bool InitSucceeded = false;
    };

  public:
    virtual ~ServiceHostBase() = default;

    ServiceHostBase(const ServiceHostBase&) = delete;
    ServiceHostBase& operator=(const ServiceHostBase&) = delete;
    ServiceHostBase(ServiceHostBase&&) = delete;
    ServiceHostBase& operator=(ServiceHostBase&&) = delete;

    /// @brief Get the io_context for this host.
    /// @return Reference to the io_context.
    virtual boost::asio::io_context& GetIoContext() = 0;

    /// @brief Get the io_context for this host (const version).
    /// @return Const reference to the io_context.
    virtual const boost::asio::io_context& GetIoContext() const = 0;

    /// @brief Process all registered services and aggregate their results.
    ///
    /// Iterates through all services registered with the provider and calls Process()
    /// on each one, merging the results according to ProcessResult priority rules.
    ///
    /// @return Aggregated ProcessResult from all services.
    ProcessResult ProcessServices()
    {
      ProcessResult result = ProcessResult::NoSleepLimit();

      auto allServices = m_provider->GetAllServiceControls();
      for (const auto& service : allServices)
      {
        result = Merge(result, service->Process());
      }

      return result;
    }

  protected:
    ServiceHostBase()
      : m_provider(std::make_shared<ManagedThreadServiceProvider>())
    {
    }

    /// @brief Implementation of service startup logic.
    /// @param services Services to start.
    /// @param currentPriority Priority level for this group.
    /// @return Awaitable that completes when services are started.
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

        // Phase 2: Initialize all services
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

    /// @brief Validate that all service records have valid factories.
    /// @param services Services to validate.
    /// @throws InvalidServiceFactoryException if any factory is null.
    void ValidateServiceFactories(const std::vector<StartServiceRecord>& services)
    {
      for (const auto& serviceRecord : services)
      {
        if (!serviceRecord.Factory)
        {
          throw InvalidServiceFactoryException(
            fmt::format("Invalid service factory in StartServiceRecord for service: {}", serviceRecord.ServiceName));
        }
      }
    }

    /// @brief Create service instances from factories.
    /// @param services Service records with factories.
    /// @param createInfo Creation info to pass to factories.
    /// @param initRecords Output vector of init records.
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
        for (const auto& typeIndex : supportedInterfaces)
        {
          record.InstanceInfo.SupportedInterfaces.push_back(typeIndex);
        }

        initRecords.push_back(std::move(record));
      }
    }

    /// @brief Initialize all services.
    /// @param initRecords Service records to initialize.
    /// @param createInfo Creation info for initialization.
    /// @return Awaitable that completes when all services have been initialized.
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

    /// @brief Process initialization results, perform rollback on failure, or register on success.
    /// @param initRecords Service init records with results.
    /// @param currentPriority Priority level for registration.
    /// @param providerProxy Proxy to clear on failure.
    /// @return Awaitable that completes when processing is done.
    /// @throws AggregateException if any services failed to initialize.
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

    /// @brief Roll back successfully initialized services on failure.
    /// @param successfulServices Services to shut down.
    /// @return Awaitable containing any exceptions that occurred during shutdown.
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

    /// @brief Register successfully initialized services with the provider.
    /// @param initRecords Service init records.
    /// @param currentPriority Priority level for registration.
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

    /// @brief Implementation of service shutdown logic for a specific priority level.
    ///
    /// Unregisters services at the given priority from the provider and shuts them down.
    /// Services within the priority group are shut down in reverse registration order.
    /// Any shutdown failures are collected and returned.
    ///
    /// @param priority The priority level to shut down.
    /// @return Awaitable containing any exceptions that occurred during shutdown.
    boost::asio::awaitable<std::vector<std::exception_ptr>> DoTryShutdownServicesAsync(ServiceLaunchPriority priority)
    {
      std::vector<std::exception_ptr> shutdownFailures;

      // Unregister services at this priority level
      auto services = m_provider->UnregisterPriorityGroup(priority);

      if (services.empty())
      {
        co_return shutdownFailures;
      }

      spdlog::info("Shutting down {} services at priority {}", services.size(), priority.GetValue());

      // Shutdown services in reverse registration order
      for (auto it = services.rbegin(); it != services.rend(); ++it)
      {
        try
        {
          auto shutdownResult = co_await it->Service->ShutdownAsync();
          if (shutdownResult != ServiceShutdownResult::Success)
          {
            spdlog::warn("Service shutdown returned non-success result: {}", static_cast<int>(shutdownResult));
          }
        }
        catch (...)
        {
          shutdownFailures.push_back(std::current_exception());
          spdlog::error("Exception during service shutdown");
        }
      }

      co_return shutdownFailures;
    }

    /// @brief Execute function on the service thread.
    /// @tparam Func Callable type.
    /// @param func Function to execute.
    /// @return Awaitable with the function result.
    template <typename Func>
    auto call(Func&& func) -> boost::asio::awaitable<decltype(std::declval<std::decay_t<Func>>()())>
    {
      using ResultType = decltype(std::declval<std::decay_t<Func>>()());

      // Use co_spawn to execute on service thread
      co_return co_await boost::asio::co_spawn(
        GetIoContext(),
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
