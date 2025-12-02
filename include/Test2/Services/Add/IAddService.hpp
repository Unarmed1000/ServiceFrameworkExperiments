#ifndef SERVICE_FRAMEWORK_TEST2_SERVICES_ADD_IADDSERVICE_HPP
#define SERVICE_FRAMEWORK_TEST2_SERVICES_ADD_IADDSERVICE_HPP
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

namespace Test2
{
  /// @brief Interface for the addition service.
  class IAddService : public IService
  {
  public:
    ~IAddService() override = default;

    /// @brief Asynchronously adds two numbers.
    /// @param a The first operand.
    /// @param b The second operand.
    /// @return An awaitable yielding the sum of a and b.
    virtual boost::asio::awaitable<double> AddAsync(double a, double b) = 0;
  };

}

#endif
