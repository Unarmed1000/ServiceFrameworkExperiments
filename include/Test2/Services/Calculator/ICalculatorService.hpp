#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_CALCULATOR_ICALCULATORSERVICE_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_CALCULATOR_ICALCULATORSERVICE_HPP
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

#include <Test2/Framework/Service/IService.hpp>
#include <boost/asio/awaitable.hpp>
#include <string>

namespace Test2
{
  /// @brief Interface for the calculator service.
  ///
  /// The calculator service parses and evaluates mathematical expressions.
  /// Supports +, -, *, /, parentheses, and proper operator precedence.
  class ICalculatorService : public IService
  {
  public:
    ~ICalculatorService() override = default;

    /// @brief Asynchronously evaluates a mathematical expression.
    /// @param expression The mathematical expression to evaluate (e.g., "2 + 3 * 4").
    /// @return An awaitable yielding the result of the expression evaluation.
    /// @throws std::invalid_argument if the expression is malformed.
    virtual boost::asio::awaitable<double> EvaluateAsync(std::string expression) = 0;
  };

}

#endif
