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

#include <Test2/Framework/Host/ServiceInstanceInfo.hpp>
#include <Test2/Framework/Host/StartedServiceInfo.hpp>
#include <Test2/Framework/Lifecycle/ILifeTracker.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Forward declarations
namespace Test2
{
  struct StartServiceRecord;
  struct StartServiceProxyRecord;
  struct ServiceCreateInfo;
  class ServiceProviderProxy;
  class ManagedThreadServiceProvider;
  class IServiceControl;
  class ProcessResult;
}

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
  /// Thread Safety:
  /// - TryStartServicesAsync() and TryShutdownServicesAsync() can be called from any thread
  /// - All other methods must be called from the service thread (m_ioContext's thread)
  class ServiceHostBase : public ILifeTracker
  {
    std::thread::id m_ownerThreadId;
    bool m_shutdownRequested{false};

  protected:
    boost::asio::io_context m_ioContext;

  public:
    std::shared_ptr<ManagedThreadServiceProvider> m_provider;

  protected:
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
    ServiceHostBase(const ServiceHostBase&) = delete;
    ServiceHostBase& operator=(const ServiceHostBase&) = delete;
    ServiceHostBase(ServiceHostBase&&) = delete;
    ServiceHostBase& operator=(ServiceHostBase&&) = delete;

    virtual ~ServiceHostBase();

    std::thread::id GetOwnerThreadId() const noexcept;

    /// @brief Get the executor for this host.
    /// @return Executor for scheduling work on this host's context.
    auto GetExecutor()
    {
      return m_ioContext.get_executor();
    }

    virtual void RequestShutdown();

    /// @brief Implementation of service startup logic.
    /// @param services Services to start.
    /// @param currentPriority Priority level for this group.
    /// @return Awaitable that completes with a vector of StartedServiceInfo for each successfully started service.
    boost::asio::awaitable<std::vector<StartedServiceInfo>> TryStartServicesAsync(std::vector<StartServiceRecord> services,
                                                                                  ServiceLaunchPriority currentPriority);

    /// @brief Creates and starts service proxies at the specified priority.
    /// @param services Service proxies to start.
    /// @param currentPriority Priority level for this group.
    boost::asio::awaitable<void> TryStartServiceProxiesAsync(std::vector<StartServiceProxyRecord> services, ServiceLaunchPriority currentPriority);

    /// @brief Called after all services and proxies have been initialized.
    /// @return Awaitable that completes when initialization is validated.
    boost::asio::awaitable<void> TryInitializeCompletedAsync();

    /// @brief Shuts down service proxies at the specified priority.
    /// @param priority The priority level to shut down.
    /// @return Awaitable containing any exceptions that occurred during shutdown.
    boost::asio::awaitable<std::vector<std::exception_ptr>> TryShutdownServiceProxiesAsync(ServiceLaunchPriority priority);

    /// @brief Shuts down services at the specified priority.
    boost::asio::awaitable<std::vector<std::exception_ptr>> TryShutdownServicesAsync(ServiceLaunchPriority priority);

  protected:
    ServiceHostBase();

    /// @brief Validates that the current thread is the owner thread.
    /// @throws WrongThreadException if called from a different thread.
    void ValidateThreadAccess() const;

    /// @brief Process all registered services and proxies.
    ///
    /// Delegates to the provider's Process() method which iterates through all instances
    /// and calls Process() on each one, merging the results according to ProcessResult priority rules.
    ///
    /// @return Aggregated ProcessResult from all services and proxies.
    ProcessResult DoProcessServices();

    std::size_t DoPoll();

    ProcessResult DoUpdate();

    void DoRun();

  private:
    /// @brief Validate that all service records have valid factories.
    /// @param services Services to validate.
    /// @throws InvalidServiceFactoryException if any factory is null.
    void ValidateServiceFactories(const std::vector<StartServiceRecord>& services);

    void ValidateServiceProxyFactories(const std::vector<StartServiceProxyRecord>& services);

    /// @brief Create service instances from factories.
    /// @param services Service records with factories.
    /// @param createInfo Creation info to pass to factories.
    /// @param initRecords Output vector of init records.
    void CreateServiceInstances(std::vector<StartServiceRecord>& services, const ServiceCreateInfo& createInfo,
                                std::vector<ServiceInitRecord>& initRecords);

    /// @brief Initialize all services.
    /// @param initRecords Service records to initialize.
    /// @param createInfo Creation info for initialization.
    /// @return Awaitable that completes when all services have been initialized.
    boost::asio::awaitable<void> InitializeServices(std::vector<ServiceInitRecord>& initRecords, const ServiceCreateInfo& createInfo);

    /// @brief Process initialization results, perform rollback on failure, or register on success.
    /// @param initRecords Service init records with results.
    /// @param currentPriority Priority level for registration.
    /// @param providerProxy Proxy to clear on failure.
    /// @return Awaitable that completes when processing is done.
    /// @throws AggregateException if any services failed to initialize.
    boost::asio::awaitable<void> ProcessInitializationResults(std::vector<ServiceInitRecord>& initRecords, ServiceLaunchPriority currentPriority,
                                                              std::shared_ptr<ServiceProviderProxy> providerProxy);

    /// @brief Roll back successfully initialized services on failure.
    /// @param successfulServices Services to shut down.
    /// @return Awaitable containing any exceptions that occurred during shutdown.
    boost::asio::awaitable<std::vector<std::exception_ptr>> RollbackServices(const std::vector<std::shared_ptr<IServiceControl>>& successfulServices);

    /// @brief Register successfully initialized services with the provider.
    /// @param initRecords Service init records.
    /// @param currentPriority Priority level for registration.
    void RegisterServicesWithProvider(std::vector<ServiceInitRecord>& initRecords, ServiceLaunchPriority currentPriority);
  };
}

#endif
