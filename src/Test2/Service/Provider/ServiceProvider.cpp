#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace Test2
{
  ServiceProvider::ServiceProvider(std::weak_ptr<IServiceProvider> provider)
    : m_provider(std::move(provider))
  {
  }

  std::shared_ptr<IService> ServiceProvider::GetService(const std::type_info& type) const
  {
    auto provider = m_provider.lock();
    if (!provider)
    {
      spdlog::error("ServiceProvider::GetService: underlying IServiceProvider has been destroyed");
      throw std::runtime_error("ServiceProvider: underlying IServiceProvider has been destroyed");
    }
    return provider->GetService(type);
  }

  std::shared_ptr<IService> ServiceProvider::TryGetService(const std::type_info& type) const
  {
    auto provider = m_provider.lock();
    if (!provider)
    {
      spdlog::warn("ServiceProvider::TryGetService: underlying IServiceProvider has been destroyed");
      return nullptr;
    }
    return provider->TryGetService(type);
  }

  bool ServiceProvider::TryGetServices(const std::type_info& type, std::vector<std::shared_ptr<IService>>& rServices) const
  {
    auto provider = m_provider.lock();
    if (!provider)
    {
      spdlog::warn("ServiceProvider::TryGetServices: underlying IServiceProvider has been destroyed");
      return false;
    }
    return provider->TryGetServices(type, rServices);
  }
}
