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
#include <Test2/Framework/Service/IService.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace Test2
{
  // Mock service for testing
  class MockService : public IService
  {
  private:
    int m_id;

  public:
    explicit MockService(int id)
      : m_id(id)
    {
    }

    [[nodiscard]] int GetId() const noexcept
    {
      return m_id;
    }
  };

  // Create helper function for service vectors
  std::vector<std::shared_ptr<IService>> CreateServices(const std::vector<int>& ids)
  {
    std::vector<std::shared_ptr<IService>> services;
    services.reserve(ids.size());
    for (int id : ids)
    {
      services.push_back(std::make_shared<MockService>(id));
    }
    return services;
  }

  // Helper to extract service IDs from a service vector
  std::vector<int> ExtractServiceIds(const std::vector<std::shared_ptr<IService>>& services)
  {
    std::vector<int> ids;
    ids.reserve(services.size());
    for (const auto& service : services)
    {
      auto mockService = std::dynamic_pointer_cast<MockService>(service);
      if (mockService)
      {
        ids.push_back(mockService->GetId());
      }
    }
    return ids;
  }
}

using namespace Test2;

// ========================================
// Normal Use Cases
// ========================================

TEST(ManagedThreadServiceProviderTest, RegisterSinglePriorityGroup)
{
  ManagedThreadServiceProvider provider;

  auto services = CreateServices({1, 2, 3});
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  // Should not throw
}

TEST(ManagedThreadServiceProviderTest, RegisterMultiplePriorityGroupsDescending)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({1, 2}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(500), CreateServices({3, 4}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(100), CreateServices({5, 6}));

  // Should not throw
}

TEST(ManagedThreadServiceProviderTest, UnregisterAllServicesReturnsReversedOrder)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({1, 2}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(500), CreateServices({3, 4}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(100), CreateServices({5, 6}));

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

  auto services = CreateServices({1, 2, 3});
  auto* originalPtr = services.data();

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  // After move, original vector should be empty or in moved-from state
  EXPECT_TRUE(services.empty() || services.data() != originalPtr);
}

TEST(ManagedThreadServiceProviderTest, ServicesAreMovedDuringUnregister)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({1, 2, 3}));

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 1);
  EXPECT_EQ(ExtractServiceIds(groups[0].Services), std::vector<int>({1, 2, 3}));
}

TEST(ManagedThreadServiceProviderTest, PreservesRegistrationOrderWithinPriorityGroup)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({1, 2, 3, 4, 5}));

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 1);
  EXPECT_EQ(ExtractServiceIds(groups[0].Services), std::vector<int>({1, 2, 3, 4, 5}));
}

TEST(ManagedThreadServiceProviderTest, UnregisterClearsInternalState)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({1, 2}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(500), CreateServices({3, 4}));

  auto groups1 = provider.UnregisterAllServices();
  EXPECT_EQ(groups1.size(), 2);

  // Second unregister should return empty
  auto groups2 = provider.UnregisterAllServices();
  EXPECT_TRUE(groups2.empty());
}

TEST(ManagedThreadServiceProviderTest, SingleServiceInPriorityGroup)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({42}));

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

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices(ids));

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
    provider.RegisterPriorityGroup(ServiceLaunchPriority(i * 10), CreateServices({i}));
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

  std::vector<std::shared_ptr<IService>> emptyServices;

  EXPECT_THROW(provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(emptyServices)), EmptyPriorityGroupException);
}

TEST(ManagedThreadServiceProviderTest, EmptyServicesVectorExceptionMessage)
{
  ManagedThreadServiceProvider provider;

  std::vector<std::shared_ptr<IService>> emptyServices;

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

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({1}));

  EXPECT_THROW(provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({2})), InvalidPriorityOrderException);
}

TEST(ManagedThreadServiceProviderTest, RegisterHigherPriorityAfterLowerThrows)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(500), CreateServices({1}));

  EXPECT_THROW(provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({2})), InvalidPriorityOrderException);
}

TEST(ManagedThreadServiceProviderTest, RegisterEqualPriorityThrows)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({1}));

  EXPECT_THROW(provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({2})), InvalidPriorityOrderException);
}

TEST(ManagedThreadServiceProviderTest, InvalidPriorityOrderExceptionMessage)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(500), CreateServices({1}));

  try
  {
    provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({2}));
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

  provider.RegisterPriorityGroup(ServiceLaunchPriority(100), CreateServices({1}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(50), CreateServices({2}));

  // This should throw because 200 > 50
  EXPECT_THROW(provider.RegisterPriorityGroup(ServiceLaunchPriority(200), CreateServices({3})), InvalidPriorityOrderException);
}

// ========================================
// Edge Cases - Priority Boundaries
// ========================================

TEST(ManagedThreadServiceProviderTest, ZeroPriorityIsValid)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(0), CreateServices({1}));

  auto groups = provider.UnregisterAllServices();
  ASSERT_EQ(groups.size(), 1);
  EXPECT_EQ(groups[0].Priority.GetValue(), 0);
}

TEST(ManagedThreadServiceProviderTest, MaxPriorityIsValid)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(UINT32_MAX), CreateServices({1}));

  auto groups = provider.UnregisterAllServices();
  ASSERT_EQ(groups.size(), 1);
  EXPECT_EQ(groups[0].Priority.GetValue(), UINT32_MAX);
}

TEST(ManagedThreadServiceProviderTest, DescendingFromMaxToZero)
{
  ManagedThreadServiceProvider provider;

  provider.RegisterPriorityGroup(ServiceLaunchPriority(UINT32_MAX), CreateServices({1}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), CreateServices({2}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(500), CreateServices({3}));
  provider.RegisterPriorityGroup(ServiceLaunchPriority(0), CreateServices({4}));

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

  auto service1 = std::make_shared<MockService>(42);
  std::weak_ptr<MockService> weakService = service1;

  std::vector<std::shared_ptr<IService>> services;
  services.push_back(service1);
  service1.reset();    // Release our reference

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  EXPECT_FALSE(weakService.expired()) << "Service should still be alive in provider";

  auto groups = provider.UnregisterAllServices();

  EXPECT_FALSE(weakService.expired()) << "Service should still be alive in returned groups";

  auto mockService = std::dynamic_pointer_cast<MockService>(groups[0].Services[0]);
  ASSERT_NE(mockService, nullptr);
  EXPECT_EQ(mockService->GetId(), 42);
}

TEST(ManagedThreadServiceProviderTest, MultipleReferencesToSameService)
{
  ManagedThreadServiceProvider provider;

  auto service = std::make_shared<MockService>(42);

  std::vector<std::shared_ptr<IService>> services;
  services.push_back(service);
  services.push_back(service);    // Same service twice
  services.push_back(service);    // And thrice

  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 1);
  ASSERT_EQ(groups[0].Services.size(), 3);

  // All three should point to the same service
  EXPECT_EQ(groups[0].Services[0].get(), groups[0].Services[1].get());
  EXPECT_EQ(groups[0].Services[1].get(), groups[0].Services[2].get());
}

// ========================================
// Edge Cases - Null Services
// ========================================

TEST(ManagedThreadServiceProviderTest, NullServicePointersAreAllowed)
{
  ManagedThreadServiceProvider provider;

  std::vector<std::shared_ptr<IService>> services;
  services.push_back(nullptr);
  services.push_back(std::make_shared<MockService>(42));
  services.push_back(nullptr);

  // Should not throw - null pointers are valid shared_ptr values
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  auto groups = provider.UnregisterAllServices();

  ASSERT_EQ(groups.size(), 1);
  ASSERT_EQ(groups[0].Services.size(), 3);
  EXPECT_EQ(groups[0].Services[0], nullptr);
  EXPECT_NE(groups[0].Services[1], nullptr);
  EXPECT_EQ(groups[0].Services[2], nullptr);
}

TEST(ManagedThreadServiceProviderTest, AllNullServicesStillCountsAsNonEmpty)
{
  ManagedThreadServiceProvider provider;

  std::vector<std::shared_ptr<IService>> services;
  services.push_back(nullptr);
  services.push_back(nullptr);

  // Should not throw EmptyPriorityGroupException because vector is not empty
  provider.RegisterPriorityGroup(ServiceLaunchPriority(1000), std::move(services));

  auto groups = provider.UnregisterAllServices();
  EXPECT_EQ(groups[0].Services.size(), 2);
}
