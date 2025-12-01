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
#include <cassert>
#include <iostream>

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

  void RunTests()
  {
    std::cout << "Running ServiceRegistry tests...\n\n";

    // Test 1: Successful factory registration
    {
      std::cout << "Test 1: Successful factory registration...";
      ServiceRegistry registry;
      auto factory = std::make_unique<MockServiceFactory>();
      registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1));
      std::cout << " PASSED\n";
    }

    // Test 2: Extract registrations
    {
      std::cout << "Test 2: Extract registrations...";
      ServiceRegistry registry;
      auto factory = std::make_unique<MockServiceFactory>();
      registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

      auto records = registry.ExtractRegistrations();
      assert(records.size() == 1);
      assert(records[0].Factory != nullptr);
      assert(records[0].Priority.GetValue() == 100);
      assert(records[0].ThreadGroupId.GetValue() == 1);
      std::cout << " PASSED\n";
    }    // Test 3: Empty registry returns empty records
    {
      std::cout << "Test 3: Empty registry returns empty records...";
      ServiceRegistry registry;
      auto records = registry.ExtractRegistrations();
      assert(records.empty());
      std::cout << " PASSED\n";
    }

    // Test 4: Multiple extractions return empty after first
    {
      std::cout << "Test 4: Multiple extractions return empty after first...";
      ServiceRegistry registry;
      auto factory = std::make_unique<MockServiceFactory>();
      registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

      auto records1 = registry.ExtractRegistrations();
      assert(records1.size() == 1);

      auto records2 = registry.ExtractRegistrations();
      assert(records2.empty());
      std::cout << " PASSED\n";
    }    // Test 5: Duplicate factory type registration throws
    {
      std::cout << "Test 5: Duplicate factory type registration throws...";
      ServiceRegistry registry;
      auto factory1 = std::make_unique<MockServiceFactory>();
      auto factory2 = std::make_unique<MockServiceFactory>();

      registry.RegisterService(std::move(factory1), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

      bool threw = false;
      try
      {
        registry.RegisterService(std::move(factory2), ServiceLaunchPriority(200), ServiceThreadGroupId(2));
      }
      catch (const DuplicateServiceRegistrationException&)
      {
        threw = true;
      }
      assert(threw);
      std::cout << " PASSED\n";
    }    // Test 6: Multiple factories with same interface are allowed (different factory types)
    {
      std::cout << "Test 6: Multiple factories with same interface (different types)...";
      ServiceRegistry registry;
      auto factory1 = std::make_unique<MockServiceFactory>();
      auto factory2 = std::make_unique<AnotherMockServiceFactory>();

      registry.RegisterService(std::move(factory1), ServiceLaunchPriority(100), ServiceThreadGroupId(1));
      registry.RegisterService(std::move(factory2), ServiceLaunchPriority(200), ServiceThreadGroupId(2));

      auto records = registry.ExtractRegistrations();
      assert(records.size() == 2);
      std::cout << " PASSED\n";
    }    // Test 7: Null factory throws
    {
      std::cout << "Test 7: Null factory throws...";
      ServiceRegistry registry;

      bool threw = false;
      try
      {
        registry.RegisterService(nullptr, ServiceLaunchPriority(100), ServiceThreadGroupId(1));
      }
      catch (const InvalidServiceFactoryException&)
      {
        threw = true;
      }
      assert(threw);
      std::cout << " PASSED\n";
    }

    // Test 8: Empty factory (zero interfaces) throws
    {
      std::cout << "Test 8: Empty factory (zero interfaces) throws...";
      ServiceRegistry registry;
      auto factory = std::make_unique<EmptyServiceFactory>();

      bool threw = false;
      try
      {
        registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), ServiceThreadGroupId(1));
      }
      catch (const InvalidServiceFactoryException&)
      {
        threw = true;
      }
      assert(threw);
      std::cout << " PASSED\n";
    }    // Test 9: Registration after extraction throws
    {
      std::cout << "Test 9: Registration after extraction throws...";
      ServiceRegistry registry;
      auto factory1 = std::make_unique<MockServiceFactory>();
      registry.RegisterService(std::move(factory1), ServiceLaunchPriority(100), ServiceThreadGroupId(1));

      auto records = registry.ExtractRegistrations();

      auto factory2 = std::make_unique<MockServiceFactory>();
      bool threw = false;
      try
      {
        registry.RegisterService(std::move(factory2), ServiceLaunchPriority(200), ServiceThreadGroupId(2));
      }
      catch (const RegistryExtractedException&)
      {
        threw = true;
      }
      assert(threw);
      std::cout << " PASSED\n";
    }

    // Test 10: Thread group ID generation
    {
      std::cout << "Test 10: Thread group ID generation...";
      ServiceRegistry registry;

      auto id1 = registry.CreateServiceThreadGroupId();
      auto id2 = registry.CreateServiceThreadGroupId();
      auto id3 = registry.CreateServiceThreadGroupId();

      assert(id1.GetValue() == 1);
      assert(id2.GetValue() == 2);
      assert(id3.GetValue() == 3);
      std::cout << " PASSED\n";
    }

    // Test 11: Thread group ID generation continues after registrations
    {
      std::cout << "Test 11: Thread group ID generation continues after registrations...";
      ServiceRegistry registry;

      auto id1 = registry.CreateServiceThreadGroupId();

      auto factory = std::make_unique<MockServiceFactory>();
      registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), id1);

      auto id2 = registry.CreateServiceThreadGroupId();
      assert(id2.GetValue() == 2);
      std::cout << " PASSED\n";
    }    // Test 12: Thread group ID generation continues after extraction
    {
      std::cout << "Test 12: Thread group ID generation continues after extraction...";
      ServiceRegistry registry;

      auto id1 = registry.CreateServiceThreadGroupId();
      auto factory = std::make_unique<MockServiceFactory>();
      registry.RegisterService(std::move(factory), ServiceLaunchPriority(100), id1);

      auto records = registry.ExtractRegistrations();

      auto id2 = registry.CreateServiceThreadGroupId();
      assert(id2.GetValue() == 2);
      std::cout << " PASSED\n";
    }    // Test 13: Different priorities and thread groups preserved
    {
      std::cout << "Test 13: Different priorities and thread groups preserved...";
      ServiceRegistry registry;

      auto factory1 = std::make_unique<MockServiceFactory>();
      auto factory2 = std::make_unique<AnotherMockServiceFactory>();

      registry.RegisterService(std::move(factory1), ServiceLaunchPriority(100), ServiceThreadGroupId(5));
      registry.RegisterService(std::move(factory2), ServiceLaunchPriority(200), ServiceThreadGroupId(10));

      auto records = registry.ExtractRegistrations();
      assert(records.size() == 2);

      // Find each record (order not guaranteed with unordered_map)
      bool found100 = false;
      bool found200 = false;
      for (const auto& record : records)
      {
        if (record.Priority.GetValue() == 100)
        {
          assert(record.ThreadGroupId.GetValue() == 5);
          found100 = true;
        }
        if (record.Priority.GetValue() == 200)
        {
          assert(record.ThreadGroupId.GetValue() == 10);
          found200 = true;
        }
      }
      assert(found100 && found200);
      std::cout << " PASSED\n";
    }
    std::cout << "\n✓ All tests passed!\n";
  }

}

int main()
{
  Test2::RunTests();
  return 0;
}
