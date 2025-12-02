#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_PROVIDER_SERVICEPROVIDER_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_PROVIDER_SERVICEPROVIDER_HPP
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

#include <Test2/Framework/Exception/ServiceProviderException.hpp>
#include <Test2/Framework/Provider/IServiceProvider.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <type_traits>
#include <vector>

namespace Test2
{
  class ServiceProvider
  {
    std::weak_ptr<IServiceProvider> m_provider;

  public:
    explicit ServiceProvider(std::weak_ptr<IServiceProvider> provider);

    std::shared_ptr<IService> GetService(const std::type_info& type) const;
    std::shared_ptr<IService> TryGetService(const std::type_info& type) const;
    bool TryGetServices(const std::type_info& type, std::vector<std::shared_ptr<IService>>& rServices) const;

    /// @brief Gets a service and casts it to the specified type.
    /// @tparam T The interface type to retrieve and cast to. Must inherit from IService.
    /// @return A shared pointer to the service cast to type T.
    /// @throws UnknownServiceException if the service is not found.
    /// @throws ServiceCastException if the cast to type T fails.
    template <typename T>
    std::shared_ptr<T> GetService() const
    {
      static_assert(std::is_base_of_v<IService, T>, "T must inherit from IService");
      auto service = GetService(typeid(T));
      auto result = std::dynamic_pointer_cast<T>(service);
      if (!result)
      {
        throw ServiceCastException(typeid(T).name(), typeid(*service).name());
      }
      return result;
    }

    /// @brief Tries to get a service and cast it to the specified type.
    /// @tparam T The interface type to retrieve and cast to. Must inherit from IService.
    /// @return A shared pointer to the service cast to type T, or nullptr if not found or cast fails.
    /// @note If a service is found but the cast fails, an error is logged as this indicates a fundamental error.
    template <typename T>
    std::shared_ptr<T> TryGetService() const
    {
      static_assert(std::is_base_of_v<IService, T>, "T must inherit from IService");
      auto service = TryGetService(typeid(T));
      if (!service)
      {
        return nullptr;
      }
      auto result = std::dynamic_pointer_cast<T>(service);
      if (!result)
      {
        spdlog::error("ServiceProvider::TryGetService: Failed to cast service from '{}' to requested type '{}'", typeid(*service).name(),
                      typeid(T).name());
        return nullptr;
      }
      return result;
    }

    /// @brief Tries to get all services of the specified type and cast them.
    /// @tparam T The interface type to retrieve and cast to. Must inherit from IService.
    /// @param rServices Reference to vector that will be populated with matching services.
    /// @return true if one or more services were found and successfully cast, false otherwise.
    /// @note If a service is found but the cast fails, an error is logged and that service is skipped.
    template <typename T>
    bool TryGetServices(std::vector<std::shared_ptr<T>>& rServices) const
    {
      static_assert(std::is_base_of_v<IService, T>, "T must inherit from IService");
      std::vector<std::shared_ptr<IService>> services;
      if (!TryGetServices(typeid(T), services))
      {
        return false;
      }
      for (const auto& service : services)
      {
        auto cast = std::dynamic_pointer_cast<T>(service);
        if (cast)
        {
          rServices.push_back(std::move(cast));
        }
        else
        {
          spdlog::error("ServiceProvider::TryGetServices: Failed to cast service from '{}' to requested type '{}'", typeid(*service).name(),
                        typeid(T).name());
        }
      }
      return !rServices.empty();
    }
  };
}

#endif