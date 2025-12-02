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

#include <Test2/Framework/Exception/ServiceCastException.hpp>
#include <Test2/Framework/Exception/UnknownServiceException.hpp>
#include <Test2/Framework/Provider/ServiceProvider.hpp>
#include <gtest/gtest.h>
#include <memory>

namespace Test2
{
  // Mock service interface for testing
  class ITestInterface1 : public IService
  {
  public:
    ~ITestInterface1() override = default;
    virtual int GetValue() const = 0;
  };

  // A second interface that our mock does NOT implement
  class ITestInterface2 : public IService
  {
  public:
    ~ITestInterface2() override = default;
    virtual std::string GetName() const = 0;
  };

  // Concrete mock service implementing ITestInterface1
  class MockServiceImpl : public ITestInterface1
  {
    int m_value;

  public:
    explicit MockServiceImpl(int value)
      : m_value(value)
    {
    }
    ~MockServiceImpl() override = default;

    int GetValue() const override
    {
      return m_value;
    }
  };

  // Mock service provider that can be configured to return specific services
  class ConfigurableMockServiceProvider : public IServiceProvider
  {
    std::shared_ptr<IService> m_service;
    std::vector<std::shared_ptr<IService>> m_services;
    bool m_returnNullOnTryGet = false;
    bool m_throwOnGet = false;

  public:
    void SetService(std::shared_ptr<IService> service)
    {
      m_service = std::move(service);
    }

    void SetServices(std::vector<std::shared_ptr<IService>> services)
    {
      m_services = std::move(services);
    }

    void SetReturnNullOnTryGet(bool value)
    {
      m_returnNullOnTryGet = value;
    }

    void SetThrowOnGet(bool value)
    {
      m_throwOnGet = value;
    }

    std::shared_ptr<IService> GetService(const std::type_info& /*type*/) const override
    {
      if (m_throwOnGet)
      {
        throw UnknownServiceException("Service not found");
      }
      return m_service;
    }

    std::shared_ptr<IService> TryGetService(const std::type_info& /*type*/) const override
    {
      if (m_returnNullOnTryGet)
      {
        return nullptr;
      }
      return m_service;
    }

    bool TryGetServices(const std::type_info& /*type*/, std::vector<std::shared_ptr<IService>>& rServices) const override
    {
      if (m_services.empty())
      {
        return false;
      }
      rServices = m_services;
      return true;
    }
  };

  // Test fixture
  class ServiceProviderTemplateTest : public ::testing::Test
  {
  protected:
    std::shared_ptr<ConfigurableMockServiceProvider> mockProvider;

    void SetUp() override
    {
      mockProvider = std::make_shared<ConfigurableMockServiceProvider>();
    }

    ServiceProvider CreateServiceProvider()
    {
      return ServiceProvider(mockProvider);
    }
  };

  // ==================== GetService<T> Tests ====================

  TEST_F(ServiceProviderTemplateTest, GetService_WhenServiceExistsAndCastSucceeds_ReturnsTypedService)
  {
    auto mockService = std::make_shared<MockServiceImpl>(42);
    mockProvider->SetService(mockService);

    ServiceProvider provider = CreateServiceProvider();
    auto result = provider.GetService<ITestInterface1>();

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->GetValue(), 42);
  }

  TEST_F(ServiceProviderTemplateTest, GetService_WhenServiceExistsButCastFails_ThrowsServiceCastException)
  {
    // Service implements ITestInterface1, but we request ITestInterface2
    auto mockService = std::make_shared<MockServiceImpl>(42);
    mockProvider->SetService(mockService);

    ServiceProvider provider = CreateServiceProvider();

    EXPECT_THROW(provider.GetService<ITestInterface2>(), ServiceCastException);
  }

  TEST_F(ServiceProviderTemplateTest, GetService_WhenServiceCastFails_ExceptionContainsTypeInfo)
  {
    auto mockService = std::make_shared<MockServiceImpl>(42);
    mockProvider->SetService(mockService);

    ServiceProvider provider = CreateServiceProvider();

    try
    {
      provider.GetService<ITestInterface2>();
      FAIL() << "Expected ServiceCastException to be thrown";
    }
    catch (const ServiceCastException& ex)
    {
      std::string message = ex.what();
      // Verify the exception message contains type information
      EXPECT_FALSE(message.empty());
      EXPECT_NE(message.find("ServiceCastException"), std::string::npos);
    }
  }

  TEST_F(ServiceProviderTemplateTest, GetService_WhenServiceNotFound_ThrowsUnknownServiceException)
  {
    mockProvider->SetThrowOnGet(true);

    ServiceProvider provider = CreateServiceProvider();

    EXPECT_THROW(provider.GetService<ITestInterface1>(), UnknownServiceException);
  }

  TEST_F(ServiceProviderTemplateTest, GetService_WhenProviderExpired_ThrowsRuntimeError)
  {
    ServiceProvider provider = CreateServiceProvider();
    mockProvider.reset();    // Expire the provider

    EXPECT_THROW(provider.GetService<ITestInterface1>(), std::runtime_error);
  }

  // ==================== TryGetService<T> Tests ====================

  TEST_F(ServiceProviderTemplateTest, TryGetService_WhenServiceExistsAndCastSucceeds_ReturnsTypedService)
  {
    auto mockService = std::make_shared<MockServiceImpl>(99);
    mockProvider->SetService(mockService);

    ServiceProvider provider = CreateServiceProvider();
    auto result = provider.TryGetService<ITestInterface1>();

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->GetValue(), 99);
  }

  TEST_F(ServiceProviderTemplateTest, TryGetService_WhenServiceNotFound_ReturnsNullptr)
  {
    mockProvider->SetReturnNullOnTryGet(true);

    ServiceProvider provider = CreateServiceProvider();
    auto result = provider.TryGetService<ITestInterface1>();

    EXPECT_EQ(result, nullptr);
  }

  TEST_F(ServiceProviderTemplateTest, TryGetService_WhenServiceExistsButCastFails_ReturnsNullptrAndLogsError)
  {
    // Service implements ITestInterface1, but we request ITestInterface2
    auto mockService = std::make_shared<MockServiceImpl>(42);
    mockProvider->SetService(mockService);

    ServiceProvider provider = CreateServiceProvider();
    // This should log an error but not throw
    auto result = provider.TryGetService<ITestInterface2>();

    EXPECT_EQ(result, nullptr);
  }

  TEST_F(ServiceProviderTemplateTest, TryGetService_WhenProviderExpired_ReturnsNullptr)
  {
    ServiceProvider provider = CreateServiceProvider();
    mockProvider.reset();    // Expire the provider

    auto result = provider.TryGetService<ITestInterface1>();

    EXPECT_EQ(result, nullptr);
  }

  // ==================== TryGetServices<T> Tests ====================

  TEST_F(ServiceProviderTemplateTest, TryGetServices_WhenServicesExistAndAllCastsSucceed_ReturnsAllTypedServices)
  {
    std::vector<std::shared_ptr<IService>> services;
    services.push_back(std::make_shared<MockServiceImpl>(1));
    services.push_back(std::make_shared<MockServiceImpl>(2));
    services.push_back(std::make_shared<MockServiceImpl>(3));
    mockProvider->SetServices(services);

    ServiceProvider provider = CreateServiceProvider();
    std::vector<std::shared_ptr<ITestInterface1>> results;
    bool success = provider.TryGetServices<ITestInterface1>(results);

    EXPECT_TRUE(success);
    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0]->GetValue(), 1);
    EXPECT_EQ(results[1]->GetValue(), 2);
    EXPECT_EQ(results[2]->GetValue(), 3);
  }

  TEST_F(ServiceProviderTemplateTest, TryGetServices_WhenNoServicesFound_ReturnsFalse)
  {
    // Empty services vector by default

    ServiceProvider provider = CreateServiceProvider();
    std::vector<std::shared_ptr<ITestInterface1>> results;
    bool success = provider.TryGetServices<ITestInterface1>(results);

    EXPECT_FALSE(success);
    EXPECT_TRUE(results.empty());
  }

  TEST_F(ServiceProviderTemplateTest, TryGetServices_WhenSomeCastsFail_ReturnsOnlySuccessfulCasts)
  {
    // Mix of services - only MockServiceImpl can be cast to ITestInterface1
    // We need another service type that implements IService but not ITestInterface1
    class OtherServiceImpl : public IService
    {
    public:
      ~OtherServiceImpl() override = default;
    };

    std::vector<std::shared_ptr<IService>> services;
    services.push_back(std::make_shared<MockServiceImpl>(1));
    services.push_back(std::make_shared<OtherServiceImpl>());    // This cast will fail
    services.push_back(std::make_shared<MockServiceImpl>(3));
    mockProvider->SetServices(services);

    ServiceProvider provider = CreateServiceProvider();
    std::vector<std::shared_ptr<ITestInterface1>> results;
    // This should log errors for failed casts but still return successful ones
    bool success = provider.TryGetServices<ITestInterface1>(results);

    EXPECT_TRUE(success);
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0]->GetValue(), 1);
    EXPECT_EQ(results[1]->GetValue(), 3);
  }

  TEST_F(ServiceProviderTemplateTest, TryGetServices_WhenAllCastsFail_ReturnsFalse)
  {
    class OtherServiceImpl : public IService
    {
    public:
      ~OtherServiceImpl() override = default;
    };

    std::vector<std::shared_ptr<IService>> services;
    services.push_back(std::make_shared<OtherServiceImpl>());
    services.push_back(std::make_shared<OtherServiceImpl>());
    mockProvider->SetServices(services);

    ServiceProvider provider = CreateServiceProvider();
    std::vector<std::shared_ptr<ITestInterface1>> results;
    // All casts fail, so should return false
    bool success = provider.TryGetServices<ITestInterface1>(results);

    EXPECT_FALSE(success);
    EXPECT_TRUE(results.empty());
  }

  TEST_F(ServiceProviderTemplateTest, TryGetServices_WhenProviderExpired_ReturnsFalse)
  {
    ServiceProvider provider = CreateServiceProvider();
    mockProvider.reset();    // Expire the provider

    std::vector<std::shared_ptr<ITestInterface1>> results;
    bool success = provider.TryGetServices<ITestInterface1>(results);

    EXPECT_FALSE(success);
    EXPECT_TRUE(results.empty());
  }

  TEST_F(ServiceProviderTemplateTest, TryGetServices_DoesNotClearProvidedVector)
  {
    std::vector<std::shared_ptr<IService>> services;
    services.push_back(std::make_shared<MockServiceImpl>(100));
    mockProvider->SetServices(services);

    ServiceProvider provider = CreateServiceProvider();

    // Pre-populate the results vector
    std::vector<std::shared_ptr<ITestInterface1>> results;
    results.push_back(std::make_shared<MockServiceImpl>(999));

    bool success = provider.TryGetServices<ITestInterface1>(results);

    EXPECT_TRUE(success);
    // Original entry should still be there, plus the new one
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0]->GetValue(), 999);    // Original
    EXPECT_EQ(results[1]->GetValue(), 100);    // Added
  }

  // ==================== ServiceCastException Tests ====================

  TEST(ServiceCastExceptionTest, What_ReturnsDescriptiveMessage)
  {
    ServiceCastException ex("RequestedType", "ActualType");

    std::string message = ex.what();
    EXPECT_NE(message.find("RequestedType"), std::string::npos);
    EXPECT_NE(message.find("ActualType"), std::string::npos);
    EXPECT_NE(message.find("ServiceCastException"), std::string::npos);
  }

  TEST(ServiceCastExceptionTest, InheritsFromBadCast)
  {
    ServiceCastException ex("RequestedType", "ActualType");

    // Should be catchable as std::bad_cast
    EXPECT_NO_THROW({
      try
      {
        throw ex;
      }
      catch (const std::bad_cast&)
      {
        // Expected
      }
    });
  }
}
