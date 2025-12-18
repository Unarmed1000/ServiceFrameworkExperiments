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
#include <Test2/Framework/Service/Async/Factory/AsyncServiceImplFactory.hpp>
#include <Test2/Framework/Service/Async/Factory/AsyncServiceProxyFactory.hpp>
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
  // Mock proxy factory for testing
  class MockProxyFactory : public AsyncServiceProxyFactory
  {
  public:
    MockProxyFactory()
      : AsyncServiceProxyFactory(typeid(IService))
    {
    }

    std::shared_ptr<IServiceProxyControl> CreateProxy(const ServiceProxyCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };

  // Mock impl factory for testing
  class MockImplFactory : public AsyncServiceImplFactory
  {
  public:
    MockImplFactory()
      : AsyncServiceImplFactory(typeid(IService))
    {
    }

    std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };

  // Another mock proxy factory with different type
  class AnotherMockProxyFactory : public AsyncServiceProxyFactory
  {
  public:
    AnotherMockProxyFactory()
      : AsyncServiceProxyFactory(typeid(IService))
    {
    }

    std::shared_ptr<IServiceProxyControl> CreateProxy(const ServiceProxyCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };

  // Another mock impl factory with different type
  class AnotherMockImplFactory : public AsyncServiceImplFactory
  {
  public:
    AnotherMockImplFactory()
      : AsyncServiceImplFactory(typeid(IService))
    {
    }

    std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };

  // Empty proxy factory (reports zero interfaces) - needs custom implementation since base requires type_index
  class EmptyProxyFactory : public IAsyncServiceProxyFactory
  {
  public:
    EmptyProxyFactory() = default;

    std::span<const std::type_index> GetSupportedInterfaces() const override
    {
      return std::span<const std::type_index>();    // Empty span
    }

    std::shared_ptr<IServiceProxyControl> CreateProxy(const ServiceProxyCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };

  // Empty impl factory (reports zero interfaces) - needs custom implementation since base requires type_index
  class EmptyImplFactory : public IAsyncServiceImplFactory
  {
  public:
    EmptyImplFactory() = default;

    std::span<const std::type_index> GetSupportedInterfaces() const override
    {
      return std::span<const std::type_index>();    // Empty span
    }

    std::shared_ptr<IServiceControl> Create(const ServiceCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };

  // Helper to create IAsyncServiceFactory for tests
  std::shared_ptr<IAsyncServiceFactory> CreateMockFactory()
  {
    return std::make_shared<AsyncServiceFactory>(std::make_shared<MockProxyFactory>(), std::make_shared<MockImplFactory>());
  }

  std::shared_ptr<IAsyncServiceFactory> CreateAnotherMockFactory()
  {
    return std::make_shared<AsyncServiceFactory>(std::make_shared<AnotherMockProxyFactory>(), std::make_shared<AnotherMockImplFactory>());
  }

  std::shared_ptr<IAsyncServiceFactory> CreateEmptyFactory()
  {
    return std::make_shared<AsyncServiceFactory>(std::make_shared<EmptyProxyFactory>(), std::make_shared<EmptyImplFactory>());
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
