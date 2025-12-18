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

#include <Test2/Framework/Exception/DuplicateServiceRegistrationException.hpp>
#include <Test2/Framework/Exception/InvalidServiceFactoryException.hpp>
#include <Test2/Framework/Exception/RegistryExtractedException.hpp>
#include <Test2/Framework/Registry/ServiceRegistry.hpp>
#include <Test2/Framework/Service/Async/Factory/AsyncServiceFactory.hpp>
#include <Test2/Framework/Service/Async/Factory/IAsyncServiceImplFactory.hpp>
#include <Test2/Framework/Service/Async/Factory/IAsyncServiceProxyFactory.hpp>
#include <Test2/Framework/Service/IServiceControl.hpp>
#include <Test2/Framework/Service/IServiceProxyControl.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <Test2/Framework/Service/ServiceProxyCreateInfo.hpp>
#include <gtest/gtest.h>
#include <typeindex>

namespace Test2
{
  // Mock unified async service factory for testing
  class MockAsyncServiceFactory : public AsyncServiceFactory
  {
  public:
    MockAsyncServiceFactory()
      : AsyncServiceFactory(typeid(IService))
    {
    }

    std::shared_ptr<IServiceProxyControl> CreateProxy(const ServiceProxyCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }

    std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };

  // Another mock unified factory with different type
  class AnotherMockAsyncServiceFactory : public AsyncServiceFactory
  {
  public:
    AnotherMockAsyncServiceFactory()
      : AsyncServiceFactory(typeid(IService))
    {
    }

    std::shared_ptr<IServiceProxyControl> CreateProxy(const ServiceProxyCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }

    std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };

  // Helper to create IAsyncServiceFactory for tests
  std::shared_ptr<IAsyncServiceFactory> CreateMockFactory()
  {
    return std::make_shared<MockAsyncServiceFactory>();
  }

  std::shared_ptr<IAsyncServiceFactory> CreateAnotherMockFactory()
  {
    return std::make_shared<AnotherMockAsyncServiceFactory>();
  }

  std::shared_ptr<IAsyncServiceFactory> CreateEmptyFactory()
  {
    // For testing empty factory error handling, we need a custom implementation
    // that returns empty interfaces. This uses IAsyncServiceFactory directly
    class EmptyAsyncServiceFactory : public IAsyncServiceFactory
    {
    public:
      std::span<const std::type_index> GetSupportedInterfaces() const override
      {
        return std::span<const std::type_index>();    // Empty span
      }

      std::shared_ptr<IServiceProxyControl> CreateProxy(const ServiceProxyCreateInfo& /*createInfo*/) override
      {
        return nullptr;
      }

      std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& /*createInfo*/) override
      {
        return nullptr;
      }

      std::type_index GetImplFactoryTypeId() const override
      {
        return typeid(*this);
      }
    };

    return std::make_shared<EmptyAsyncServiceFactory>();
  }
}

using namespace Test2;

TEST(ServiceRegistryTest, SuccessfulFactoryRegistration)
{
  ServiceRegistry registry;
  auto factory = CreateMockFactory();
  registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1));
  // No exception means success
}

TEST(ServiceRegistryTest, ExtractRegistrations)
{
  ServiceRegistry registry;
  auto factory = CreateMockFactory();
  registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

  auto records = registry.ExtractRegistrations();
  ASSERT_EQ(records.size(), 1);
  EXPECT_NE(records[0].Factory, nullptr);
  EXPECT_EQ(records[0].Priority.GetValue(), 100);
  EXPECT_EQ(records[0].ThreadGroupId.GetValue(), 1);
}

TEST(ServiceRegistryTest, EmptyRegistryReturnsEmptyRecords)
{
  ServiceRegistry registry;
  auto records = registry.ExtractRegistrations();
  EXPECT_TRUE(records.empty());
}

TEST(ServiceRegistryTest, MultipleExtractionsReturnEmptyAfterFirst)
{
  ServiceRegistry registry;
  auto factory = CreateMockFactory();
  registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

  auto records1 = registry.ExtractRegistrations();
  ASSERT_EQ(records1.size(), 1);

  auto records2 = registry.ExtractRegistrations();
  EXPECT_TRUE(records2.empty());
}

TEST(ServiceRegistryTest, DuplicateFactoryTypeThrows)
{
  ServiceRegistry registry;
  auto factory1 = CreateMockFactory();
  auto factory2 = CreateMockFactory();

  registry.RegisterService(std::move(factory1), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

  EXPECT_THROW(registry.RegisterService(std::move(factory2), ServiceLaunchPriority(200), ServiceThreadGroupId(2)),
               DuplicateServiceRegistrationException);
}

TEST(ServiceRegistryTest, MultipleDifferentFactoryTypes)
{
  ServiceRegistry registry;
  auto factory1 = CreateMockFactory();
  auto factory2 = CreateAnotherMockFactory();

  registry.RegisterService(std::move(factory1), ServiceLaunchPriority(100), ServiceThreadGroupId(1));
  registry.RegisterService(std::move(factory2), ServiceLaunchPriority(200), ServiceThreadGroupId(2));

  auto records = registry.ExtractRegistrations();
  EXPECT_EQ(records.size(), 2);
}

TEST(ServiceRegistryTest, NullFactoryThrows)
{
  ServiceRegistry registry;
  EXPECT_THROW(registry.RegisterService(nullptr, ServiceLaunchPriority(100), ServiceThreadGroupId(1)), InvalidServiceFactoryException);
}

TEST(ServiceRegistryTest, EmptyFactoryThrows)
{
  ServiceRegistry registry;
  auto factory = CreateEmptyFactory();

  EXPECT_THROW(registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1)), InvalidServiceFactoryException);
}

TEST(ServiceRegistryTest, RegistrationAfterExtractionThrows)
{
  ServiceRegistry registry;
  auto factory1 = CreateMockFactory();
  registry.RegisterService(std::move(factory1), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

  auto records = registry.ExtractRegistrations();

  auto factory2 = CreateMockFactory();
  EXPECT_THROW(registry.RegisterService(std::move(factory2), ServiceLaunchPriority(200), ServiceThreadGroupId(2)), RegistryExtractedException);
}

TEST(ServiceRegistryTest, ThreadGroupIdGeneration)
{
  ServiceRegistry registry;

  auto id1 = registry.CreateServiceThreadGroupId();
  auto id2 = registry.CreateServiceThreadGroupId();
  auto id3 = registry.CreateServiceThreadGroupId();

  EXPECT_EQ(id1.GetValue(), 1);
  EXPECT_EQ(id2.GetValue(), 2);
  EXPECT_EQ(id3.GetValue(), 3);
}

TEST(ServiceRegistryTest, ThreadGroupIdContinuesAfterRegistrations)
{
  ServiceRegistry registry;

  auto id1 = registry.CreateServiceThreadGroupId();

  auto factory = CreateMockFactory();
  registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), id1);

  auto id2 = registry.CreateServiceThreadGroupId();
  EXPECT_EQ(id2.GetValue(), 2);
}

TEST(ServiceRegistryTest, ThreadGroupIdContinuesAfterExtraction)
{
  ServiceRegistry registry;

  auto id1 = registry.CreateServiceThreadGroupId();
  auto factory = CreateMockFactory();
  registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), id1);

  auto records = registry.ExtractRegistrations();

  auto id2 = registry.CreateServiceThreadGroupId();
  EXPECT_EQ(id2.GetValue(), 2);
}

TEST(ServiceRegistryTest, PrioritiesAndThreadGroupsPreserved)
{
  ServiceRegistry registry;

  auto factory1 = CreateMockFactory();
  auto factory2 = CreateAnotherMockFactory();

  registry.RegisterService(std::move(factory1), ServiceLaunchPriority(100), ServiceThreadGroupId(5));
  registry.RegisterService(std::move(factory2), ServiceLaunchPriority(200), ServiceThreadGroupId(10));

  auto records = registry.ExtractRegistrations();
  ASSERT_EQ(records.size(), 2);

  // Find each record (order not guaranteed with unordered_map)
  bool found100 = false;
  bool found200 = false;
  for (const auto& record : records)
  {
    if (record.Priority.GetValue() == 100)
    {
      EXPECT_EQ(record.ThreadGroupId.GetValue(), 5);
      found100 = true;
    }
    if (record.Priority.GetValue() == 200)
    {
      EXPECT_EQ(record.ThreadGroupId.GetValue(), 10);
      found200 = true;
    }
  }
  EXPECT_TRUE(found100);
  EXPECT_TRUE(found200);
}
