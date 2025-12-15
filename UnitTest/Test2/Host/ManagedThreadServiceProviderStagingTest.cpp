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

#include <Test2/Framework/Host/Managed/ManagedThreadServiceProvider.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Framework/Service/IService.hpp>
#include <Test2/Framework/Service/IServiceControlBase.hpp>
#include <Test2/Framework/Service/ProcessResult.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <typeindex>

namespace Test2
{
  // Mock service for testing
  class MockStagingService
    : public IService
    , public IServiceControlBase
  {
  public:
    ProcessResult Process() override
    {
      return ProcessResult::NoSleepLimit();
    }
  };

  // Test: Services registered but not committed should not be visible
  TEST(ManagedThreadServiceProviderStaging, StagedRegistration_ServicesNotVisibleBeforeCommit)
  {
    ManagedThreadServiceProvider provider;

    // Create mock service
    auto service = std::make_shared<MockStagingService>();
    std::vector<ServiceProviderServiceInstance> instances;
    instances.push_back(ServiceProviderServiceInstance{InstanceType::Service, service, {std::type_index(typeid(IService))}});

    // Register at priority 100 - should stage, not commit
    provider.RegisterPriorityGroup(InstanceType::Service, ServiceLaunchPriority(100), std::move(instances));

    // Service should NOT be visible yet (not committed)
    EXPECT_EQ(provider.GetServiceCount(), 0);
    EXPECT_EQ(provider.TryGetService(typeid(IService)), nullptr);
  }

  // Test: Committing staged services makes them visible
  TEST(ManagedThreadServiceProviderStaging, CommitStagedPriority_MakesServicesVisible)
  {
    ManagedThreadServiceProvider provider;

    // Create and register service
    auto service = std::make_shared<MockStagingService>();
    std::vector<ServiceProviderServiceInstance> instances;
    instances.push_back(ServiceProviderServiceInstance{InstanceType::Service, service, {std::type_index(typeid(IService))}});

    provider.RegisterPriorityGroup(InstanceType::Service, ServiceLaunchPriority(100), std::move(instances));

    // Commit staged services
    provider.CommitStagedPriority();

    // Now service should be visible
    EXPECT_EQ(provider.GetServiceCount(), 1);
    EXPECT_NE(provider.TryGetService(typeid(IService)), nullptr);
  }

  // Test: Discarding staged services removes them without committing
  TEST(ManagedThreadServiceProviderStaging, DiscardStagedPriority_ClearsStaging)
  {
    ManagedThreadServiceProvider provider;

    // Create and register service
    auto service = std::make_shared<MockStagingService>();
    std::vector<ServiceProviderServiceInstance> instances;
    instances.push_back(ServiceProviderServiceInstance{InstanceType::Service, service, {std::type_index(typeid(IService))}});

    provider.RegisterPriorityGroup(InstanceType::Service, ServiceLaunchPriority(100), std::move(instances));

    // Discard instead of commit
    provider.DiscardStagedPriority();

    // Service should NOT be visible
    EXPECT_EQ(provider.GetServiceCount(), 0);
    EXPECT_EQ(provider.TryGetService(typeid(IService)), nullptr);
  }

  // Test: Can stage multiple services at same priority
  TEST(ManagedThreadServiceProviderStaging, StageMultipleServices_AllCommittedTogether)
  {
    ManagedThreadServiceProvider provider;

    // Create and register first service
    auto service1 = std::make_shared<MockStagingService>();
    std::vector<ServiceProviderServiceInstance> instances1;
    instances1.push_back(ServiceProviderServiceInstance{InstanceType::Service, service1, {std::type_index(typeid(IService))}});
    provider.RegisterPriorityGroup(InstanceType::Service, ServiceLaunchPriority(100), std::move(instances1));

    // Register second service at same priority
    auto service2 = std::make_shared<MockStagingService>();
    std::vector<ServiceProviderServiceInstance> instances2;
    instances2.push_back(ServiceProviderServiceInstance{InstanceType::Service, service2, {std::type_index(typeid(IService))}});
    provider.RegisterPriorityGroup(InstanceType::Service, ServiceLaunchPriority(100), std::move(instances2));

    // Both should be staged but not visible
    EXPECT_EQ(provider.GetServiceCount(), 0);

    // Commit
    provider.CommitStagedPriority();

    // Both should now be visible
    EXPECT_EQ(provider.GetServiceCount(), 2);
  }

  // Phase 5: Idempotent Discard Tests

  // Test: Calling DiscardStagedPriority twice should not throw
  TEST(ManagedThreadServiceProviderStaging, DiscardStagedPriority_CalledTwice_NoException)
  {
    ManagedThreadServiceProvider provider;

    // Create and register service
    auto service = std::make_shared<MockStagingService>();
    std::vector<ServiceProviderServiceInstance> instances;
    instances.push_back(ServiceProviderServiceInstance{InstanceType::Service, service, {std::type_index(typeid(IService))}});
    provider.RegisterPriorityGroup(InstanceType::Service, ServiceLaunchPriority(100), std::move(instances));

    // Discard once
    EXPECT_NO_THROW(provider.DiscardStagedPriority());

    // Discard again - should not throw (idempotent)
    EXPECT_NO_THROW(provider.DiscardStagedPriority());

    // Service should still not be visible
    EXPECT_EQ(provider.GetServiceCount(), 0);
  }

  // Test: Calling DiscardStagedPriority when empty should not throw
  TEST(ManagedThreadServiceProviderStaging, DiscardStagedPriority_WhenEmpty_NoException)
  {
    ManagedThreadServiceProvider provider;

    // No services staged - discard should be no-op
    EXPECT_NO_THROW(provider.DiscardStagedPriority());

    // Should still be empty
    EXPECT_EQ(provider.GetServiceCount(), 0);
  }

  // Test: Commit followed by discard on empty staging
  TEST(ManagedThreadServiceProviderStaging, CommitThenDiscard_EmptyStaging_NoException)
  {
    ManagedThreadServiceProvider provider;

    // Create and register service
    auto service = std::make_shared<MockStagingService>();
    std::vector<ServiceProviderServiceInstance> instances;
    instances.push_back(ServiceProviderServiceInstance{InstanceType::Service, service, {std::type_index(typeid(IService))}});
    provider.RegisterPriorityGroup(InstanceType::Service, ServiceLaunchPriority(100), std::move(instances));

    // Commit staging
    provider.CommitStagedPriority();

    // Discard on empty staging - should not throw
    EXPECT_NO_THROW(provider.DiscardStagedPriority());

    // Service should still be visible (was committed)
    EXPECT_EQ(provider.GetServiceCount(), 1);
  }
}
