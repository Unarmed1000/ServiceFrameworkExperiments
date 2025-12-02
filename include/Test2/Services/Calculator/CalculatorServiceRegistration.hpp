#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_CALCULATOR_CALCULATORSERVICEREGISTRATION_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_CALCULATOR_CALCULATORSERVICEREGISTRATION_HPP
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

#include <Test2/Framework/Registry/IServiceRegistry.hpp>
#include <Test2/Framework/Registry/ServiceLaunchPriority.hpp>
#include <Test2/Services/Add/AddServiceFactory.hpp>
#include <Test2/Services/Calculator/CalculatorServiceFactory.hpp>
#include <Test2/Services/Divide/DivideServiceFactory.hpp>
#include <Test2/Services/Multiply/MultiplyServiceFactory.hpp>
#include <Test2/Services/Subtract/SubtractServiceFactory.hpp>
#include <memory>

namespace Test2
{
  /// @brief Registers all calculator-related services with the service registry.
  ///
  /// This function registers the math services (Add, Subtract, Multiply, Divide) at a higher
  /// priority (200) so they are initialized first. The CalculatorService is registered at a
  /// lower priority (100) so it can acquire its dependencies during construction.
  ///
  /// Each service runs in its own thread group for isolated execution.
  ///
  /// @param registry The service registry to register services with.
  ///
  /// Example usage:
  /// @code
  /// ServiceRegistry registry;
  /// RegisterCalculatorServices(registry);
  /// auto registrations = registry.ExtractRegistrations();
  /// // Process registrations to initialize services...
  /// @endcode
  inline void RegisterCalculatorServices(IServiceRegistry& registry)
  {
    // Create unique thread groups for each service
    const auto addThreadGroup = registry.CreateServiceThreadGroupId();
    const auto subtractThreadGroup = registry.CreateServiceThreadGroupId();
    const auto multiplyThreadGroup = registry.CreateServiceThreadGroupId();
    const auto divideThreadGroup = registry.CreateServiceThreadGroupId();
    const auto calculatorThreadGroup = registry.CreateServiceThreadGroupId();

    // Priority 200 for math services (initialized first - higher priority = earlier initialization)
    constexpr ServiceLaunchPriority MathServicePriority(200);

    // Priority 100 for calculator service (initialized after dependencies - lower priority = later initialization)
    constexpr ServiceLaunchPriority CalculatorServicePriority(100);

    // Register math services at higher priority so they are available when CalculatorService is created
    registry.RegisterService(std::make_unique<AddServiceFactory>(), MathServicePriority, addThreadGroup);
    registry.RegisterService(std::make_unique<SubtractServiceFactory>(), MathServicePriority, subtractThreadGroup);
    registry.RegisterService(std::make_unique<MultiplyServiceFactory>(), MathServicePriority, multiplyThreadGroup);
    registry.RegisterService(std::make_unique<DivideServiceFactory>(), MathServicePriority, divideThreadGroup);

    // Register calculator service at lower priority so dependencies are resolved first
    registry.RegisterService(std::make_unique<CalculatorServiceFactory>(), CalculatorServicePriority, calculatorThreadGroup);
  }

}

#endif
