#ifndef SERVICE_FRAMEWORK_TEST1_CALCULATORSERVICE_HPP
#define SERVICE_FRAMEWORK_TEST1_CALCULATORSERVICE_HPP
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

#include <Test1/AddService.hpp>
#include <Test1/DivideService.hpp>
#include <Test1/MultiplyService.hpp>
#include <Test1/ServiceBase.hpp>
#include <Test1/SubtractService.hpp>
#include <spdlog/spdlog.h>
#include <cctype>
#include <stdexcept>
#include <string>

namespace Test1
{
  // Calculator Service - parses and evaluates math expressions
  // Supports +, -, *, /, parentheses, and proper operator precedence
  class CalculatorService : public ServiceBase
  {
  private:
    AddService& m_addService;
    MultiplyService& m_multiplyService;
    SubtractService& m_subtractService;
    DivideService& m_divideService;

    // Parser context - local to each evaluation to support concurrent calls
    struct ParserContext
    {
      std::string expression;
      size_t position;

      ParserContext(const std::string& expr)
        : expression(expr)
        , position(0)
      {
      }

      void skipWhitespace()
      {
        while (position < expression.length() && std::isspace(expression[position]))
        {
          position++;
        }
      }

      char peek()
      {
        skipWhitespace();
        if (position >= expression.length())
          return '\0';
        return expression[position];
      }

      char consume()
      {
        skipWhitespace();
        if (position >= expression.length())
          return '\0';
        return expression[position++];
      }

      static bool isDigit(char c)
      {
        return c >= '0' && c <= '9';
      }
    };

    // Recursive descent parser methods
    boost::asio::awaitable<double> parseNumber(ParserContext& ctx)
    {
      ctx.skipWhitespace();
      std::string numStr;
      bool hasDecimal = false;
      bool isNegative = false;

      // Handle negative numbers
      if (ctx.peek() == '-')
      {
        isNegative = true;
        ctx.consume();
      }

      while (ctx.position < ctx.expression.length())
      {
        char c = ctx.expression[ctx.position];
        if (ParserContext::isDigit(c))
        {
          numStr += c;
          ctx.position++;
        }
        else if (c == '.' && !hasDecimal)
        {
          hasDecimal = true;
          numStr += c;
          ctx.position++;
        }
        else if (std::isspace(c))
        {
          ctx.position++;
          break;
        }
        else
        {
          break;
        }
      }

      if (numStr.empty() || numStr == ".")
      {
        throw std::invalid_argument("Invalid number format at position " + std::to_string(ctx.position));
      }

      double value = std::stod(numStr);
      if (isNegative)
        value = -value;

      co_return value;
    }

    // Parse primary expression: number or (expression)
    boost::asio::awaitable<double> parsePrimary(ParserContext& ctx)
    {
      char c = ctx.peek();

      if (c == '(')
      {
        ctx.consume();    // consume '('
        double result = co_await parseExpression(ctx);
        if (ctx.consume() != ')')
        {
          throw std::invalid_argument("Missing closing parenthesis");
        }
        co_return result;
      }
      else if (ParserContext::isDigit(c) || c == '.' || c == '-')
      {
        co_return co_await parseNumber(ctx);
      }
      else
      {
        throw std::invalid_argument(std::string("Unexpected character: ") + c);
      }
    }

    // Parse multiplication and division (higher precedence)
    boost::asio::awaitable<double> parseTerm(ParserContext& ctx)
    {
      double left = co_await parsePrimary(ctx);

      while (true)
      {
        char op = ctx.peek();
        if (op == '*')
        {
          ctx.consume();
          double right = co_await parsePrimary(ctx);
          left = co_await m_multiplyService.MultiplyAsync(left, right);
        }
        else if (op == '/')
        {
          ctx.consume();
          double right = co_await parsePrimary(ctx);
          left = co_await m_divideService.DivideAsync(left, right);
        }
        else
        {
          break;
        }
      }

      co_return left;
    }

    // Parse addition and subtraction (lower precedence)
    boost::asio::awaitable<double> parseExpression(ParserContext& ctx)
    {
      double left = co_await parseTerm(ctx);

      while (true)
      {
        char op = ctx.peek();
        if (op == '+')
        {
          ctx.consume();
          double right = co_await parseTerm(ctx);
          left = co_await m_addService.AddAsync(left, right);
        }
        else if (op == '-')
        {
          ctx.consume();
          double right = co_await parseTerm(ctx);
          left = co_await m_subtractService.SubtractAsync(left, right);
        }
        else
        {
          break;
        }
      }

      co_return left;
    }

  public:
    CalculatorService(AddService& add, MultiplyService& mul, SubtractService& sub, DivideService& div)
      : ServiceBase("CalculatorService")
      , m_addService(add)
      , m_multiplyService(mul)
      , m_subtractService(sub)
      , m_divideService(div)
    {
    }

    boost::asio::awaitable<double> EvaluateAsync(std::string expression)
    {
      spdlog::info("[CalculatorService] Evaluating: {}", expression);

      // Create parser context local to this evaluation (thread-safe for concurrent calls)
      ParserContext ctx(expression);

      // Parse and evaluate
      double result = co_await parseExpression(ctx);

      // Check if we consumed the entire expression
      ctx.skipWhitespace();
      if (ctx.position < ctx.expression.length())
      {
        throw std::invalid_argument("Unexpected characters at end of expression at position " + std::to_string(ctx.position));
      }

      spdlog::info("[CalculatorService] Result: {}", result);
      co_return result;
    }
  };
}

#endif
