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

#include <Test2/Framework/Host/EmptyPriorityGroupException.hpp>
#include <Test2/Framework/Host/InvalidPriorityOrderException.hpp>
#include <Test2/Framework/Host/ManagedThreadServiceProvider.hpp>
#include <Test2/Framework/Host/ServiceInstanceInfo.hpp>
#include <Test2/Framework/Provider/ServiceProviderException.hpp>
#include <Test2/Framework/Service/IService.hpp>
#include <Test2/Framework/Service/IServiceControl.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <Test2/Framework/Service/ServiceInitResult.hpp>
#include <Test2/Framework/Service/ServiceProcessResult.hpp>
#include <Test2/Framework/Service/ServiceShutdownResult.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <memory>
#include <span>
#include <vector>

namespace Test2
{
  // Mock service for testing
  class MockServiceControl : public IServiceControl
  {
  private:
    int m_id;

  public:
    explicit MockServiceControl(int id)
      : m_id(id)
    {
    }

    [[nodiscard]] int GetId() const noexcept
    {
      return m_id;
    }

    boost::asio::awaitable<ServiceInitResult> InitAsync(const ServiceCreateInfo& /*creationInfo*/) override
    {
      co_return ServiceInitResult::Success;
    }

    boost::asio::awaitable<ServiceShutdownResult> ShutdownAsync(const ServiceCreateInfo& /*creationInfo*/) override
    {
      co_return ServiceShutdownResult::Success;
    }

    ServiceProcessResult Process() override
    {
      return ServiceProcessResult{};
    }
  };

  // Interface types for testing
  struct ITestInterface1 : public IService
  {
  };
  struct ITestInterface2 : public IService
  {
  };
  struct ITestInterface3 : public IService
  {
  };

  // Create helper function for service instance info vectors with default interfaces
  std::vector<Test2::ServiceInstanceInfo> CreateServices(const std::vector<int>& ids)
  {
    std::vector<Test2::ServiceInstanceInfo> services;
    services.reserve(ids.size());
    for (int id : ids)
    {
      services.push_back({std::make_shared<MockServiceControl>(id), {&typeid(IService), &typeid(ITestInterface1)}});
    }
    return services;
  }

  // Helper to create supported interfaces for services
  // Each service supports IService and ITestInterface1 by default
  std::vector<std::vector<const std::type_info*>> CreateDefaultInterfaces(size_t count)
  {
    std::vector<std::vector<const std::type_info*>> interfaces;
    interfaces.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
      interfaces.push_back({&typeid(IService), &typeid(ITestInterface1)});
    }
    return interfaces;
  }

  // Helper to convert vector of vectors to span of spans
  std::vector<std::span<const std::type_info* const>> ToSpanOfSpans(const std::vector<std::vector<const std::type_info*>>& interfaces)
  {
    std::vector<std::span<const std::type_info* const>> spans;
    spans.reserve(interfaces.size());
    for (const auto& vec : interfaces)
    {
      spans.emplace_back(vec.data(), vec.size());
    }
    return spans;
  }

  // Helper to extract service IDs from ServiceInstanceInfo vector
  std::vector<int> ExtractServiceIds(const std::vector<Test2::ServiceInstanceInfo>& services)
  {
    std::vector<int> ids;
    ids.reserve(services.size());
    for (const auto& info : services)
    {
      auto mockService = std::dynamic_pointer_cast<MockServiceControl>(info.Service);
      if (mockService)
      {
        ids.push_back(mockService->GetId());
      }
    }
    return ids;
  }

  // Helper to register services with default interfaces (IService and ITestInterface1)
  void RegisterWithDefaults(ManagedThreadServiceProvider& provider, ServiceLaunchPriority priority, const std::vector<int>& ids)
  {
    std::vector<Test2::ServiceInstanceInfo> serviceInfos;
    serviceInfos.reserve(ids.size());

    for (int id : ids)
    {
      Test2::ServiceInstanceInfo info;
      info.Service = std::make_shared<MockServiceControl>(id);
      info.SupportedInterfaces = {&typeid(IService), &typeid(ITestInterface1)};
      serviceInfos.push_back(std::move(info));
    }

    provider.RegisterPriorityGroup(priority, std::move(serviceInfos));
  }
}

using namespace Test2;

// ========================================
// Normal Use Cases
// ========================================

TEST(ManagedThreadServiceProviderTest, RegisterSinglePriorityGroup)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {1, 2, 3});

  // Should not throw
}

TEST(ManagedThreadServiceProviderTest, RegisterMultiplePriorityGroupsDescending)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {1, 2});
  RegisterWithDefaults(provider, ServiceLaunchPriority(500), {3, 4});
  RegisterWithDefaults(provider, ServiceLaunchPriority(100), {5, 6});

  // Should not throw
}

TEST(ManagedThreadServiceProviderTest, UnregisterAllServicesReturnsReversedOrder)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {1, 2});
  RegisterWithDefaults(provider, ServiceLaunchPriority(500), {3, 4});
  RegisterWithDefaults(provider, ServiceLaunchPriority(100), {5, 6});

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 3);

  // Verify shutdown order: low to high priority
  EXPECT_EQ(groups[0].Priority.GetValue(), 100);
  EXPECT_EQ(groups[1].Priority.GetValue(), 500);
  EXPECT_EQ(groups[2].Priority.GetValue(), 1000);

  // Verify service IDs
  EXPECT_EQ(ExtractServiceIds(groups[0].Services), std::vector<int>({5, 6}));
  EXPECT_EQ(ExtractServiceIds(groups[1].Services), std::vector<int>({3, 4}));
  EXPECT_EQ(ExtractServiceIds(groups[2].Services), std::vector<int>({1, 2}));
}

TEST(ManagedThreadServiceProviderTest, ServicesAreMovedDuringRegistration)
{
  ManagedThreadServiceProvider provider;

  std::vector<Test2::ServiceInstanceInfo> serviceInfos;
  for (int id : {1, 2, 3})
  {
    Test2::ServiceInstanceInfo info;
    info.Service = std::make_shared<MockServiceControl>(id);
    info.SupportedInterfaces = {&typeid(IService), &typeid(ITestInterface1)};
    serviceInfos.push_back(std::move(info));
  }
  auto* originalPtr = serviceInfos.data();

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(serviceInfos));

  // After move, original vector should be empty or in moved-from state
  EXPECT_TRUE(serviceInfos.empty() || serviceInfos.data() != originalPtr);
}

TEST(ManagedThreadServiceProviderTest, ServicesAreMovedDuringUnregister)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {1, 2, 3});

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 1);
  EXPECT_EQ(ExtractServiceIds(groups[0].Services), std::vector<int>({1, 2, 3}));
}

TEST(ManagedThreadServiceProviderTest, PreservesRegistrationOrderWithinPriorityGroup)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {1, 2, 3, 4, 5});

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 1);
  EXPECT_EQ(ExtractServiceIds(groups[0].Services), std::vector<int>({1, 2, 3, 4, 5}));
}

TEST(ManagedThreadServiceProviderTest, UnregisterClearsInternalState)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {1, 2});
  RegisterWithDefaults(provider, ServiceLaunchPriority(500), {3, 4});

  auto groups1 = provider.UnregisterAllServices();
  EXPECT_EQ(groups1.size(), 2);

  // Second unregister should return empty
  auto groups2 = provider.UnregisterAllServices();
  EXPECT_TRUE(groups2.empty());
}

TEST(ManagedThreadServiceProviderTest, SingleServiceInPriorityGroup)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {42});

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 1);
  EXPECT_EQ(groups[0].Priority.GetValue(), 1000);
  EXPECT_EQ(ExtractServiceIds(groups[0].Services), std::vector<int>({42}));
}

TEST(ManagedThreadServiceProviderTest, LargeNumberOfServicesInGroup)
{
  ManagedThreadServiceProvider provider;

  std::vector<int> ids(1000);
  for (int i = 0; i < 1000; ++i)
  {
    ids[i] = i;
  }

  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), ids);

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 1);
  EXPECT_EQ(ExtractServiceIds(groups[0].Services), ids);
}

TEST(ManagedThreadServiceProviderTest, LargeNumberOfPriorityGroups)
{
  ManagedThreadServiceProvider provider;

  const int numGroups = 100;
  for (int i = numGroups; i > 0; --i)
  {
    RegisterWithDefaults(provider, ServiceLaunchPriority(i * 10), {i});
  }

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), numGroups);

  // Verify reverse order
  for (int i = 0; i < numGroups; ++i)
  {
    EXPECT_EQ(groups[i].Priority.GetValue(), (i + 1) * 10);
  }
}

// ========================================
// Edge Cases - Empty States
// ========================================

TEST(ManagedThreadServiceProviderTest, UnregisterWithoutAnyRegistrations)
{
  ManagedThreadServiceProvider provider;

  auto groups = provider.UnregisterAllServices();

  EXPECT_TRUE(groups.empty());
}

TEST(ManagedThreadServiceProviderTest, MultipleUnregistersWhenEmpty)
{
  ManagedThreadServiceProvider provider;

  auto groups1 = provider.UnregisterAllServices();
  auto groups2 = provider.UnregisterAllServices();
  auto groups3 = provider.UnregisterAllServices();

  EXPECT_TRUE(groups1.empty());
  EXPECT_TRUE(groups2.empty());
  EXPECT_TRUE(groups3.empty());
}

// ========================================
// Edge Cases - Empty Services Vector
// ========================================

TEST(ManagedThreadServiceProviderTest, EmptyServicesVectorThrows)
{
  ManagedThreadServiceProvider provider;

  std::vector<Test2::ServiceInstanceInfo> emptyServices;

  EXPECT_THROW(provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(emptyServices)), EmptyPriorityGroupException);
}

TEST(ManagedThreadServiceProviderTest, EmptyServicesVectorExceptionMessage)
{
  ManagedThreadServiceProvider provider;

  std::vector<Test2::ServiceInstanceInfo> emptyServices;

  try
  {
    provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(emptyServices));
    FAIL() << "Expected EmptyPriorityGroupException";
  }
  catch (const EmptyPriorityGroupException& e)
  {
    std::string message = e.what();
    EXPECT_TRUE(message.find("1000") != std::string::npos) << "Exception message should contain priority value: " << message;
  }
}

// ========================================
// Edge Cases - Priority Order Violations
// ========================================

TEST(ManagedThreadServiceProviderTest, RegisterSamePriorityTwiceThrows)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {1});

  EXPECT_THROW(RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {2}), InvalidPriorityOrderException);
}

TEST(ManagedThreadServiceProviderTest, RegisterHigherPriorityAfterLowerThrows)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(500), {1});

  EXPECT_THROW(RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {2}), InvalidPriorityOrderException);
}

TEST(ManagedThreadServiceProviderTest, RegisterEqualPriorityThrows)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {1});

  EXPECT_THROW(RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {2}), InvalidPriorityOrderException);
}

TEST(ManagedThreadServiceProviderTest, InvalidPriorityOrderExceptionMessage)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(500), {1});

  try
  {
    RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {2});
    FAIL() << "Expected InvalidPriorityOrderException";
  }
  catch (const InvalidPriorityOrderException& e)
  {
    std::string message = e.what();
    EXPECT_TRUE(message.find("1000") != std::string::npos) << "Exception message should contain new priority: " << message;
    EXPECT_TRUE(message.find("500") != std::string::npos) << "Exception message should contain last priority: " << message;
    EXPECT_TRUE(message.find("decreasing") != std::string::npos || message.find("high to low") != std::string::npos)
      << "Exception message should mention ordering requirement: " << message;
  }
}

TEST(ManagedThreadServiceProviderTest, RegisterIncrementingPrioritiesThrows)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(100), {1});
  RegisterWithDefaults(provider, ServiceLaunchPriority(50), {2});

  // This should throw because 200 > 50
  EXPECT_THROW(RegisterWithDefaults(provider, ServiceLaunchPriority(200), {3}), InvalidPriorityOrderException);
}

// ========================================
// Edge Cases - Priority Boundaries
// ========================================

TEST(ManagedThreadServiceProviderTest, ZeroPriorityIsValid)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(0), {1});

  auto groups = provider.UnregisterAllServices();
  ASSERT_EQ(groups.size(), 1);
  EXPECT_EQ(groups[0].Priority.GetValue(), 0);
}

TEST(ManagedThreadServiceProviderTest, MaxPriorityIsValid)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(UINT32_MAX), {1});

  auto groups = provider.UnregisterAllServices();
  ASSERT_EQ(groups.size(), 1);
  EXPECT_EQ(groups[0].Priority.GetValue(), UINT32_MAX);
}

TEST(ManagedThreadServiceProviderTest, DescendingFromMaxToZero)
{
  ManagedThreadServiceProvider provider;

  RegisterWithDefaults(provider, ServiceLaunchPriority(UINT32_MAX), {1});
  RegisterWithDefaults(provider, ServiceLaunchPriority(1000), {2});
  RegisterWithDefaults(provider, ServiceLaunchPriority(500), {3});
  RegisterWithDefaults(provider, ServiceLaunchPriority(0), {4});

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 4);
  EXPECT_EQ(groups[0].Priority.GetValue(), 0);
  EXPECT_EQ(groups[1].Priority.GetValue(), 500);
  EXPECT_EQ(groups[2].Priority.GetValue(), 1000);
  EXPECT_EQ(groups[3].Priority.GetValue(), UINT32_MAX);
}

TEST(ManagedThreadServiceProviderTest, ConsecutivePriorities)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(100), CreateServices({1}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(99), CreateServices({2}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(98), CreateServices({3}));

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 3);
  EXPECT_EQ(groups[0].Priority.GetValue(), 98);
  EXPECT_EQ(groups[1].Priority.GetValue(), 99);
  EXPECT_EQ(groups[2].Priority.GetValue(), 100);
}

// ========================================
// Edge Cases - First Registration
// ========================================

TEST(ManagedThreadServiceProviderTest, FirstRegistrationCanBeAnyPriority)
{
  ManagedThreadServiceProvider provider1;
  provider1.RegisterPriorityGroup(ServiceLaunchPriority(0), CreateServices({1}));

  ManagedThreadServiceProvider provider2;
  provider2.RegisterPriorityGroup(ServiceLaunchPriority(UINT32_MAX), CreateServices({1}));

  ManagedThreadServiceProvider provider3;
  provider3.RegisterPriorityGroup(ServiceLaunchPriority(500), CreateServices({1}));

  // All should succeed
}

// ========================================
// Edge Cases - Complex Scenarios
// ========================================

TEST(ManagedThreadServiceProviderTest, RegisterUnregisterRegisterAgain)
{
  ManagedThreadServiceProvider provider;

  // First cycle
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({1, 2}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(500), CreateServices({3, 4}));

  auto groups1 = provider.UnregisterAllServices();
  EXPECT_EQ(groups1.size(), 2);

  // Second cycle - should work fine after unregister
  provider.RegisterPriorityGroup(ServiceLaunchPriority(2000), CreateServices({5, 6}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1500), CreateServices({7, 8}));

  auto groups2 = provider.UnregisterAllServices();
  ASSERT_EQ(groups2.size(), 2);
  EXPECT_EQ(groups2[0].Priority.GetValue(), 1500);
  EXPECT_EQ(groups2[1].Priority.GetValue(), 2000);
}

TEST(ManagedThreadServiceProviderTest, MixedServiceCountsPerGroup)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({1}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(900), CreateServices({2, 3}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(800), CreateServices({4, 5, 6}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(700), CreateServices({7, 8, 9, 10}));

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 4);
  EXPECT_EQ(groups[0].Services.size(), 4);    // Priority 700
  EXPECT_EQ(groups[1].Services.size(), 3);    // Priority 800
  EXPECT_EQ(groups[2].Services.size(), 2);    // Priority 900
  EXPECT_EQ(groups[3].Services.size(), 1);    // Priority 1000
}

// ========================================
// Edge Cases - Service Lifetime
// ========================================

TEST(ManagedThreadServiceProviderTest, ServicePointersPersistAfterUnregister)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(42);
  std::weak_ptr<MockServiceControl> weakService = service1;

  std::vector<Test2::ServiceInstanceInfo> serviceInfos;
  Test2::ServiceInstanceInfo info;
  info.Service = service1;
  info.SupportedInterfaces = {&typeid(IService), &typeid(ITestInterface1)};
  serviceInfos.push_back(std::move(info));
  service1.reset();    // Release our reference

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(serviceInfos));

  EXPECT_FALSE(weakService.expired()) << "Service should still be alive in provider";

  auto groups = provider.UnregisterAllServices();

  EXPECT_FALSE(weakService.expired()) << "Service should still be alive in returned groups";

  auto mockService = std::dynamic_pointer_cast<MockServiceControl>(groups[0].Services[0].Service);
  ASSERT_NE(mockService, nullptr);
  EXPECT_EQ(mockService->GetId(), 42);
}

TEST(ManagedThreadServiceProviderTest, MultipleReferencesToSameService)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);

  std::vector<Test2::ServiceInstanceInfo> serviceInfos;
  for (int i = 0; i < 3; ++i)
  {
    Test2::ServiceInstanceInfo info;
    info.Service = service;    // Same service instance
    info.SupportedInterfaces = {&typeid(IService), &typeid(ITestInterface1)};
    serviceInfos.push_back(std::move(info));
  }

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(serviceInfos));

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 1);
  ASSERT_EQ(groups[0].Services.size(), 3);

  // All three should point to the same service
  EXPECT_EQ(groups[0].Services[0].Service.get(), groups[0].Services[1].Service.get());
  EXPECT_EQ(groups[0].Services[1].Service.get(), groups[0].Services[2].Service.get());
}

// ========================================
// Edge Cases - Null Services
// ========================================

TEST(ManagedThreadServiceProviderTest, NullServicePointersThrow)
{
  ManagedThreadServiceProvider provider;

  std::vector<Test2::ServiceInstanceInfo> serviceInfos;

  Test2::ServiceInstanceInfo info1;
  info1.Service = nullptr;    // Null service
  info1.SupportedInterfaces = {&typeid(IService)};
  serviceInfos.push_back(std::move(info1));

  Test2::ServiceInstanceInfo info2;
  info2.Service = std::make_shared<MockServiceControl>(42);
  info2.SupportedInterfaces = {&typeid(IService)};
  serviceInfos.push_back(std::move(info2));

  // Should throw because first service is null
  EXPECT_THROW(provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(serviceInfos)), std::invalid_argument);
}

TEST(ManagedThreadServiceProviderTest, ServiceWithNoInterfacesThrows)
{
  ManagedThreadServiceProvider provider;

  std::vector<Test2::ServiceInstanceInfo> serviceInfos;

  Test2::ServiceInstanceInfo info;
  info.Service = std::make_shared<MockServiceControl>(42);
  info.SupportedInterfaces = {};    // No interfaces
  serviceInfos.push_back(std::move(info));
  // Should throw because service has no supported interfaces
  EXPECT_THROW(provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(serviceInfos)), std::invalid_argument);
}

// ========================================
// IServiceProvider Tests - GetService
// ========================================

TEST(ManagedThreadServiceProviderTest, GetServiceReturnsRegisteredService)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(ITestInterface1)}}});

  auto retrieved = provider.GetService(typeid(ITestInterface1));
  ASSERT_NE(retrieved, nullptr);

  auto mockService = std::dynamic_pointer_cast<MockServiceControl>(retrieved);
  ASSERT_NE(mockService, nullptr);
  EXPECT_EQ(mockService->GetId(), 42);
}

TEST(ManagedThreadServiceProviderTest, GetServiceThrowsForUnknownType)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(ITestInterface1)}}});

  EXPECT_THROW(provider.GetService(typeid(ITestInterface2)), UnknownServiceException);
}

TEST(ManagedThreadServiceProviderTest, GetServiceThrowsForMultipleServices)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(1);
  auto service2 = std::make_shared<MockServiceControl>(2);

  std::vector<Test2::ServiceInstanceInfo> services;
  services.push_back({service1, {&typeid(ITestInterface1)}});
  services.push_back({service2, {&typeid(ITestInterface1)}});

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  EXPECT_THROW(provider.GetService(typeid(ITestInterface1)), MultipleServicesFoundException);
}

TEST(ManagedThreadServiceProviderTest, GetServiceExceptionMessageContainsTypeName)
{
  ManagedThreadServiceProvider provider;

  try
  {
    provider.GetService(typeid(ITestInterface1));
    FAIL() << "Expected UnknownServiceException";
  }
  catch (const UnknownServiceException& ex)
  {
    std::string message = ex.what();
    EXPECT_NE(message.find("ITestInterface1"), std::string::npos);
  }
}

TEST(ManagedThreadServiceProviderTest, GetServiceMultipleServicesExceptionMessage)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(1);
  auto service2 = std::make_shared<MockServiceControl>(2);

  std::vector<Test2::ServiceInstanceInfo> services;
  services.push_back({service1, {&typeid(ITestInterface1)}});
  services.push_back({service2, {&typeid(ITestInterface1)}});

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  try
  {
    provider.GetService(typeid(ITestInterface1));
    FAIL() << "Expected MultipleServicesFoundException";
  }
  catch (const MultipleServicesFoundException& ex)
  {
    std::string message = ex.what();
    EXPECT_NE(message.find("Multiple services"), std::string::npos);
    EXPECT_NE(message.find("TryGetServices"), std::string::npos);
  }
}

TEST(ManagedThreadServiceProviderTest, GetServiceWorksWithIService)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(IService)}}});

  auto retrieved = provider.GetService(typeid(IService));
  ASSERT_NE(retrieved, nullptr);

  auto mockService = std::dynamic_pointer_cast<MockServiceControl>(retrieved);
  ASSERT_NE(mockService, nullptr);
  EXPECT_EQ(mockService->GetId(), 42);
}

TEST(ManagedThreadServiceProviderTest, GetServiceWorksAcrossPriorityGroups)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(1);
  auto service2 = std::make_shared<MockServiceControl>(2);

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service1, {&typeid(ITestInterface1)}}});
  provider.RegisterPriorityGroup(ServiceLaunchPriority(500), {{service2, {&typeid(ITestInterface2)}}});

  auto retrieved1 = provider.GetService(typeid(ITestInterface1));
  auto retrieved2 = provider.GetService(typeid(ITestInterface2));

  auto mock1 = std::dynamic_pointer_cast<MockServiceControl>(retrieved1);
  auto mock2 = std::dynamic_pointer_cast<MockServiceControl>(retrieved2);

  ASSERT_NE(mock1, nullptr);
  ASSERT_NE(mock2, nullptr);
  EXPECT_EQ(mock1->GetId(), 1);
  EXPECT_EQ(mock2->GetId(), 2);
}

// ========================================
// IServiceProvider Tests - TryGetService
// ========================================

TEST(ManagedThreadServiceProviderTest, TryGetServiceReturnsRegisteredService)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(ITestInterface1)}}});

  auto retrieved = provider.TryGetService(typeid(ITestInterface1));
  ASSERT_NE(retrieved, nullptr);

  auto mockService = std::dynamic_pointer_cast<MockServiceControl>(retrieved);
  ASSERT_NE(mockService, nullptr);
  EXPECT_EQ(mockService->GetId(), 42);
}

TEST(ManagedThreadServiceProviderTest, TryGetServiceReturnsNullForUnknownType)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(ITestInterface1)}}});

  auto retrieved = provider.TryGetService(typeid(ITestInterface2));
  EXPECT_EQ(retrieved, nullptr);
}

TEST(ManagedThreadServiceProviderTest, TryGetServiceReturnsFirstWhenMultiple)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(1);
  auto service2 = std::make_shared<MockServiceControl>(2);

  std::vector<Test2::ServiceInstanceInfo> services;
  services.push_back({service1, {&typeid(ITestInterface1)}});
  services.push_back({service2, {&typeid(ITestInterface1)}});

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  // Should return one of them (first found in the map)
  auto retrieved = provider.TryGetService(typeid(ITestInterface1));
  ASSERT_NE(retrieved, nullptr);

  auto mockService = std::dynamic_pointer_cast<MockServiceControl>(retrieved);
  ASSERT_NE(mockService, nullptr);
  // Should be either 1 or 2
  EXPECT_TRUE(mockService->GetId() == 1 || mockService->GetId() == 2);
}

TEST(ManagedThreadServiceProviderTest, TryGetServiceReturnsNullOnEmptyProvider)
{
  ManagedThreadServiceProvider provider;

  auto retrieved = provider.TryGetService(typeid(ITestInterface1));
  EXPECT_EQ(retrieved, nullptr);
}

// ========================================
// IServiceProvider Tests - TryGetServices
// ========================================

TEST(ManagedThreadServiceProviderTest, TryGetServicesReturnsSingleService)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(ITestInterface1)}}});

  std::vector<std::shared_ptr<IService>> services;
  bool result = provider.TryGetServices(typeid(ITestInterface1), services);

  EXPECT_TRUE(result);
  ASSERT_EQ(services.size(), 1);

  auto mockService = std::dynamic_pointer_cast<MockServiceControl>(services[0]);
  ASSERT_NE(mockService, nullptr);
  EXPECT_EQ(mockService->GetId(), 42);
}

TEST(ManagedThreadServiceProviderTest, TryGetServicesReturnsMultipleServices)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(1);
  auto service2 = std::make_shared<MockServiceControl>(2);
  auto service3 = std::make_shared<MockServiceControl>(3);

  std::vector<Test2::ServiceInstanceInfo> serviceInfos;
  serviceInfos.push_back({service1, {&typeid(ITestInterface1)}});
  serviceInfos.push_back({service2, {&typeid(ITestInterface1)}});
  serviceInfos.push_back({service3, {&typeid(ITestInterface1)}});

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(serviceInfos));

  std::vector<std::shared_ptr<IService>> services;
  bool result = provider.TryGetServices(typeid(ITestInterface1), services);

  EXPECT_TRUE(result);
  ASSERT_EQ(services.size(), 3);

  // Collect IDs
  std::vector<int> ids;
  for (const auto& svc : services)
  {
    auto mockService = std::dynamic_pointer_cast<MockServiceControl>(svc);
    ASSERT_NE(mockService, nullptr);
    ids.push_back(mockService->GetId());
  }

  std::sort(ids.begin(), ids.end());
  EXPECT_EQ(ids[0], 1);
  EXPECT_EQ(ids[1], 2);
  EXPECT_EQ(ids[2], 3);
}

TEST(ManagedThreadServiceProviderTest, TryGetServicesReturnsFalseForUnknownType)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(ITestInterface1)}}});

  std::vector<std::shared_ptr<IService>> services;
  bool result = provider.TryGetServices(typeid(ITestInterface2), services);

  EXPECT_FALSE(result);
  EXPECT_EQ(services.size(), 0);
}

TEST(ManagedThreadServiceProviderTest, TryGetServicesAppendsToExistingVector)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(1);
  auto service2 = std::make_shared<MockServiceControl>(2);

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service1, {&typeid(ITestInterface1)}}});

  std::vector<std::shared_ptr<IService>> services;
  services.push_back(service2);    // Pre-existing entry

  bool result = provider.TryGetServices(typeid(ITestInterface1), services);

  EXPECT_TRUE(result);
  ASSERT_EQ(services.size(), 2);    // Pre-existing + new
}

TEST(ManagedThreadServiceProviderTest, TryGetServicesAcrossPriorityGroups)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(1);
  auto service2 = std::make_shared<MockServiceControl>(2);

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service1, {&typeid(ITestInterface1)}}});
  provider.RegisterPriorityGroup(ServiceLaunchPriority(500), {{service2, {&typeid(ITestInterface1)}}});

  std::vector<std::shared_ptr<IService>> services;
  bool result = provider.TryGetServices(typeid(ITestInterface1), services);

  EXPECT_TRUE(result);
  ASSERT_EQ(services.size(), 2);
}

TEST(ManagedThreadServiceProviderTest, TryGetServicesReturnsFalseOnEmptyProvider)
{
  ManagedThreadServiceProvider provider;

  std::vector<std::shared_ptr<IService>> services;
  bool result = provider.TryGetServices(typeid(ITestInterface1), services);

  EXPECT_FALSE(result);
  EXPECT_EQ(services.size(), 0);
}

// ========================================
// IServiceProvider Tests - Multiple Interface Support
// ========================================

TEST(ManagedThreadServiceProviderTest, ServiceWithMultipleInterfacesAccessibleByEach)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(IService), &typeid(ITestInterface1), &typeid(ITestInterface2)}}});

  auto byIService = provider.GetService(typeid(IService));
  auto byInterface1 = provider.GetService(typeid(ITestInterface1));
  auto byInterface2 = provider.GetService(typeid(ITestInterface2));

  EXPECT_EQ(byIService, service);
  EXPECT_EQ(byInterface1, service);
  EXPECT_EQ(byInterface2, service);
}

TEST(ManagedThreadServiceProviderTest, DifferentServicesForDifferentInterfaces)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(1);
  auto service2 = std::make_shared<MockServiceControl>(2);

  std::vector<Test2::ServiceInstanceInfo> services;
  services.push_back({service1, {&typeid(ITestInterface1)}});
  services.push_back({service2, {&typeid(ITestInterface2)}});

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  auto retrieved1 = provider.GetService(typeid(ITestInterface1));
  auto retrieved2 = provider.GetService(typeid(ITestInterface2));

  auto mock1 = std::dynamic_pointer_cast<MockServiceControl>(retrieved1);
  auto mock2 = std::dynamic_pointer_cast<MockServiceControl>(retrieved2);

  ASSERT_NE(mock1, nullptr);
  ASSERT_NE(mock2, nullptr);
  EXPECT_EQ(mock1->GetId(), 1);
  EXPECT_EQ(mock2->GetId(), 2);
}

TEST(ManagedThreadServiceProviderTest, MultipleServicesWithOverlappingInterfaces)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(1);
  auto service2 = std::make_shared<MockServiceControl>(2);

  std::vector<Test2::ServiceInstanceInfo> services;
  services.push_back({service1, {&typeid(ITestInterface1), &typeid(ITestInterface2)}});
  services.push_back({service2, {&typeid(ITestInterface2), &typeid(ITestInterface3)}});

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  // ITestInterface1 - only service1
  auto interface1 = provider.GetService(typeid(ITestInterface1));
  auto mock1 = std::dynamic_pointer_cast<MockServiceControl>(interface1);
  ASSERT_NE(mock1, nullptr);
  EXPECT_EQ(mock1->GetId(), 1);

  // ITestInterface2 - both service1 and service2
  EXPECT_THROW(provider.GetService(typeid(ITestInterface2)), MultipleServicesFoundException);

  std::vector<std::shared_ptr<IService>> interface2Services;
  provider.TryGetServices(typeid(ITestInterface2), interface2Services);
  EXPECT_EQ(interface2Services.size(), 2);

  // ITestInterface3 - only service2
  auto interface3 = provider.GetService(typeid(ITestInterface3));
  auto mock3 = std::dynamic_pointer_cast<MockServiceControl>(interface3);
  ASSERT_NE(mock3, nullptr);
  EXPECT_EQ(mock3->GetId(), 2);
}

// ========================================
// IServiceProvider Tests - Unregister Effects
// ========================================

TEST(ManagedThreadServiceProviderTest, GetServiceThrowsAfterUnregister)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(ITestInterface1)}}});

  // Should work before unregister
  EXPECT_NO_THROW(provider.GetService(typeid(ITestInterface1)));

  // Unregister all
  provider.UnregisterAllServices();

  // Should throw after unregister
  EXPECT_THROW(provider.GetService(typeid(ITestInterface1)), UnknownServiceException);
}

TEST(ManagedThreadServiceProviderTest, TryGetServiceReturnsNullAfterUnregister)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(ITestInterface1)}}});

  // Should work before unregister
  ASSERT_NE(provider.TryGetService(typeid(ITestInterface1)), nullptr);

  // Unregister all
  provider.UnregisterAllServices();

  // Should return null after unregister
  EXPECT_EQ(provider.TryGetService(typeid(ITestInterface1)), nullptr);
}

TEST(ManagedThreadServiceProviderTest, TryGetServicesReturnsFalseAfterUnregister)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockServiceControl>(42);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(ITestInterface1)}}});

  std::vector<std::shared_ptr<IService>> services;
  EXPECT_TRUE(provider.TryGetServices(typeid(ITestInterface1), services));
  EXPECT_EQ(services.size(), 1);

  // Unregister all
  provider.UnregisterAllServices();

  // Should return false after unregister
  services.clear();
  EXPECT_FALSE(provider.TryGetServices(typeid(ITestInterface1), services));
  EXPECT_EQ(services.size(), 0);
}

TEST(ManagedThreadServiceProviderTest, CanReregisterAfterUnregister)
{
  ManagedThreadServiceProvider provider;

  auto service1 = std::make_shared<MockServiceControl>(1);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service1, {&typeid(ITestInterface1)}}});

  auto retrieved1 = provider.GetService(typeid(ITestInterface1));
  auto mock1 = std::dynamic_pointer_cast<MockServiceControl>(retrieved1);
  EXPECT_EQ(mock1->GetId(), 1);

  // Unregister
  provider.UnregisterAllServices();

  // Register different service
  auto service2 = std::make_shared<MockServiceControl>(2);
  provider.RegisterPriorityGroup(ServiceLaunchPriority(2000), {{service2, {&typeid(ITestInterface1)}}});

  auto retrieved2 = provider.GetService(typeid(ITestInterface1));
  auto mock2 = std::dynamic_pointer_cast<MockServiceControl>(retrieved2);
  EXPECT_EQ(mock2->GetId(), 2);
}

// ========================================
// IServiceProvider Tests - Edge Cases
// ========================================

TEST(ManagedThreadServiceProviderTest, GetServiceWithManyServicesOfDifferentTypes)
{
  ManagedThreadServiceProvider provider;

  std::vector<Test2::ServiceInstanceInfo> services;
  for (int i = 0; i < 100; ++i)
  {
    auto service = std::make_shared<MockServiceControl>(i);
    // Each service has a unique interface (using IService as a proxy for different types)
    services.push_back({service, {&typeid(IService)}});
  }

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  // Should throw because multiple services support IService
  EXPECT_THROW(provider.GetService(typeid(IService)), MultipleServicesFoundException);
}

TEST(ManagedThreadServiceProviderTest, TryGetServicesWithManyMatchingServices)
{
  ManagedThreadServiceProvider provider;

  std::vector<Test2::ServiceInstanceInfo> services;
  for (int i = 0; i < 100; ++i)
  {
    auto service = std::make_shared<MockServiceControl>(i);
    services.push_back({service, {&typeid(ITestInterface1)}});
  }

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  std::vector<std::shared_ptr<IService>> retrievedServices;
  bool result = provider.TryGetServices(typeid(ITestInterface1), retrievedServices);

  EXPECT_TRUE(result);
  EXPECT_EQ(retrievedServices.size(), 100);
}

TEST(ManagedThreadServiceProviderTest, ServiceLookupPreservesServiceLifetime)
{
  ManagedThreadServiceProvider provider;

  std::weak_ptr<MockServiceControl> weakService;

  {
    auto service = std::make_shared<MockServiceControl>(42);
    weakService = service;
    provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), {{service, {&typeid(ITestInterface1)}}});
  }

  // Service should still be alive because provider holds it
  EXPECT_FALSE(weakService.expired());

  // Can retrieve it
  auto retrieved = provider.GetService(typeid(ITestInterface1));
  ASSERT_NE(retrieved, nullptr);

  // After unregister and clearing retrieved pointer, should be destroyed
  provider.UnregisterAllServices();
  retrieved.reset();

  EXPECT_TRUE(weakService.expired());
}
