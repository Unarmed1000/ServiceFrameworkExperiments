#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_CALCULATOR_CALCULATORSERVICE_IMPL_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_CALCULATOR_CALCULATORSERVICE_IMPL_HPP
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

#include <Test2/Framework/Service/Async/AsyncServiceBase.hpp>
#include <Test2/Framework/Service/ServiceCreateInfo.hpp>
#include <Test2/Services/Add/IAddService.hpp>
#include <Test2/Services/Calculator/ICalculatorService.hpp>
#include <Test2/Services/Divide/IDivideService.hpp>
#include <Test2/Services/Multiply/IMultiplyService.hpp>
#include <Test2/Services/Subtract/ISubtractService.hpp>
#include <memory>
#include <string>

namespace Test2
{
  /// @brief Calculator Service - parses and evaluates math expressions.
  ///
  /// Supports +, -, *, /, parentheses, and proper operator precedence.
  /// Uses dependency injection to acquire the math services via ServiceProvider.
  class CalculatorService final
    : public AsyncServiceBase
    , public ICalculatorService
  {
  private:
    std::shared_ptr<IAddService> m_addService;
    std::shared_ptr<IMultiplyService> m_multiplyService;
    std::shared_ptr<ISubtractService> m_subtractService;
    std::shared_ptr<IDivideService> m_divideService;

    /// @brief Parser context - local to each evaluation to support concurrent calls.
    struct ParserContext
    {
      std::string expression;
      size_t position;

      explicit ParserContext(std::string expr);
      void skipWhitespace();
      char peek();
      char consume();
      static bool isDigit(char c);
    };

    boost::asio::awaitable<double> parseNumber(ParserContext& ctx);
    boost::asio::awaitable<double> parsePrimary(ParserContext& ctx);
    boost::asio::awaitable<double> parseTerm(ParserContext& ctx);
    boost::asio::awaitable<double> parseExpression(ParserContext& ctx);

  public:
    explicit CalculatorService(const ServiceCreateInfo& createInfo);
    ~CalculatorService() override = default;

    boost::asio::awaitable<double> EvaluateAsync(std::string expression) final;
  };

}

#endif
