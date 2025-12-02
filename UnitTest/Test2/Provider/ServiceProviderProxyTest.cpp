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
#include <Test2/Framework/Provider/ServiceProviderProxy.hpp>
#include <gtest/gtest.h>
#include <memory>

namespace Test2
{
  // Mock service for testing
  class MockService : public IService
  {
  public:
    ~MockService() override = default;
  };

  // Mock service provider for testing
  class MockServiceProvider : public IServiceProvider
  {
    std::shared_ptr<IService> m_service = std::make_shared<MockService>();

  public:
    std::shared_ptr<IService> GetService(const std::type_info& type) const override
    {
      return m_service;
    }

    std::shared_ptr<IService> TryGetService(const std::type_info& type) const override
    {
      return m_service;
    }

    bool TryGetServices(const std::type_info& type, std::vector<std::shared_ptr<IService>>& rServices) const override
    {
      rServices.push_back(m_service);
      return true;
    }
  };

  // Test fixture
  class ServiceProviderProxyTest : public ::testing::Test
  {
  protected:
    std::shared_ptr<MockServiceProvider> mockProvider;
    std::shared_ptr<ServiceProviderProxy> proxy;

    void SetUp() override
    {
      mockProvider = std::make_shared<MockServiceProvider>();
      proxy = std::make_shared<ServiceProviderProxy>(mockProvider);
    }
  };

  // Tests for GetService
  TEST_F(ServiceProviderProxyTest, GetService_WhenValid_DelegatesToUnderlyingProvider)
  {
    auto service = proxy->GetService(typeid(MockService));
    ASSERT_NE(service, nullptr);
  }

  TEST_F(ServiceProviderProxyTest, GetService_WhenCleared_ThrowsException)
  {
    proxy->Clear();
    EXPECT_THROW(proxy->GetService(typeid(MockService)), ServiceProviderException);
  }

  // Tests for TryGetService
  TEST_F(ServiceProviderProxyTest, TryGetService_WhenValid_DelegatesToUnderlyingProvider)
  {
    auto service = proxy->TryGetService(typeid(MockService));
    ASSERT_NE(service, nullptr);
  }

  TEST_F(ServiceProviderProxyTest, TryGetService_WhenCleared_ReturnsNullptr)
  {
    proxy->Clear();
    auto service = proxy->TryGetService(typeid(MockService));
    EXPECT_EQ(service, nullptr);
  }

  // Tests for TryGetServices
  TEST_F(ServiceProviderProxyTest, TryGetServices_WhenValid_DelegatesToUnderlyingProvider)
  {
    std::vector<std::shared_ptr<IService>> services;
    bool result = proxy->TryGetServices(typeid(MockService), services);
    EXPECT_TRUE(result);
    EXPECT_EQ(services.size(), 1);
  }

  TEST_F(ServiceProviderProxyTest, TryGetServices_WhenCleared_ReturnsFalse)
  {
    proxy->Clear();
    std::vector<std::shared_ptr<IService>> services;
    bool result = proxy->TryGetServices(typeid(MockService), services);
    EXPECT_FALSE(result);
    EXPECT_TRUE(services.empty());
  }

  // Test multiple Clear calls
  TEST_F(ServiceProviderProxyTest, Clear_CalledMultipleTimes_IsSafe)
  {
    proxy->Clear();
    proxy->Clear();
    proxy->Clear();

    // Verify proxy is still in cleared state
    EXPECT_THROW(proxy->GetService(typeid(MockService)), ServiceProviderException);
    EXPECT_EQ(proxy->TryGetService(typeid(MockService)), nullptr);
  }
}
