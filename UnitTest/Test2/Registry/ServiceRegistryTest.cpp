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

#include <Test2/Framework/Registry/ServiceRegistry.hpp>
#include <Test2/Framework/Registry/ServiceRegistryException.hpp>
#include <Test2/Framework/Service/IServiceFactory.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <gtest/gtest.h>

namespace Test2
{
  // Mock service factory for testing
  class MockServiceFactory : public IServiceFactory
  {
  public:
    MockServiceFactory() = default;

    std::span<const std::type_info> GetSupportedInterfaces() const override
    {
      static const std::type_info* interfaces[] = {&typeid(IService)};
      return std::span<const std::type_info>(reinterpret_cast<const std::type_info*>(interfaces), 1);
    }

    std::shared_ptr<IServiceControl> Create(const std::type_info& /*type*/, const ServiceCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };

  // Another mock factory with different type
  class AnotherMockServiceFactory : public IServiceFactory
  {
  public:
    AnotherMockServiceFactory() = default;

    std::span<const std::type_info> GetSupportedInterfaces() const override
    {
      static const std::type_info* interfaces[] = {&typeid(IService)};
      return std::span<const std::type_info>(reinterpret_cast<const std::type_info*>(interfaces), 1);
    }

    std::shared_ptr<IServiceControl> Create(const std::type_info& /*type*/, const ServiceCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };

  // Empty factory (reports zero interfaces)
  class EmptyServiceFactory : public IServiceFactory
  {
  public:
    EmptyServiceFactory() = default;

    std::span<const std::type_info> GetSupportedInterfaces() const override
    {
      return std::span<const std::type_info>();    // Empty span
    }

    std::shared_ptr<IServiceControl> Create(const std::type_info& /*type*/, const ServiceCreateInfo& /*createInfo*/) override
    {
      return nullptr;
    }
  };
}

using namespace Test2;

TEST(ServiceRegistryTest, SuccessfulFactoryRegistration)
{
  ServiceRegistry registry;
  auto factory = std::make_unique<MockServiceFactory>();
  registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1));
  // No exception means success
}

TEST(ServiceRegistryTest, ExtractRegistrations)
{
  ServiceRegistry registry;
  auto factory = std::make_unique<MockServiceFactory>();
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
  auto factory = std::make_unique<MockServiceFactory>();
  registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

  auto records1 = registry.ExtractRegistrations();
  ASSERT_EQ(records1.size(), 1);

  auto records2 = registry.ExtractRegistrations();
  EXPECT_TRUE(records2.empty());
}

TEST(ServiceRegistryTest, DuplicateFactoryTypeThrows)
{
  ServiceRegistry registry;
  auto factory1 = std::make_unique<MockServiceFactory>();
  auto factory2 = std::make_unique<MockServiceFactory>();

  registry.RegisterService(std::move(factory1), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

  EXPECT_THROW(registry.RegisterService(std::move(factory2), ServiceLaunchPriority(200), ServiceThreadGroupId(2)),
               DuplicateServiceRegistrationException);
}

TEST(ServiceRegistryTest, MultipleDifferentFactoryTypes)
{
  ServiceRegistry registry;
  auto factory1 = std::make_unique<MockServiceFactory>();
  auto factory2 = std::make_unique<AnotherMockServiceFactory>();

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
  auto factory = std::make_unique<EmptyServiceFactory>();

  EXPECT_THROW(registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1)), InvalidServiceFactoryException);
}

TEST(ServiceRegistryTest, RegistrationAfterExtractionThrows)
{
  ServiceRegistry registry;
  auto factory1 = std::make_unique<MockServiceFactory>();
  registry.RegisterService(std::move(factory1), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

  auto records = registry.ExtractRegistrations();

  auto factory2 = std::make_unique<MockServiceFactory>();
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

  auto factory = std::make_unique<MockServiceFactory>();
  registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), id1);

  auto id2 = registry.CreateServiceThreadGroupId();
  EXPECT_EQ(id2.GetValue(), 2);
}

TEST(ServiceRegistryTest, ThreadGroupIdContinuesAfterExtraction)
{
  ServiceRegistry registry;

  auto id1 = registry.CreateServiceThreadGroupId();
  auto factory = std::make_unique<MockServiceFactory>();
  registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), id1);

  auto records = registry.ExtractRegistrations();

  auto id2 = registry.CreateServiceThreadGroupId();
  EXPECT_EQ(id2.GetValue(), 2);
}

TEST(ServiceRegistryTest, PrioritiesAndThreadGroupsPreserved)
{
  ServiceRegistry registry;

  auto factory1 = std::make_unique<MockServiceFactory>();
  auto factory2 = std::make_unique<AnotherMockServiceFactory>();

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
