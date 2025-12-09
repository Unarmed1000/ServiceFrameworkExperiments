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

#include <Common/AggregateException.hpp>
#include <sstream>
#include <typeinfo>

namespace Common
{
  // Static helper methods
  std::string AggregateException::GenerateMessage(const std::string& message, const std::vector<std::exception_ptr>& /*exceptions*/)
  {
    if (!message.empty())
    {
      return message;
    }
    return "One or more errors occurred.";
  }

  void AggregateException::ValidateNonEmpty(const std::vector<std::exception_ptr>& exceptions)
  {
    if (exceptions.empty())
    {
      throw std::invalid_argument("The innerExceptions argument must contain at least one exception.");
    }
  }

  void AggregateException::FlattenHelper(const std::vector<std::exception_ptr>& exceptions, std::vector<std::exception_ptr>& result)
  {
    for (const auto& exPtr : exceptions)
    {
      try
      {
        if (exPtr)
        {
          std::rethrow_exception(exPtr);
        }
      }
      catch (const AggregateException& aggEx)
      {
        // Recursively flatten nested AggregateExceptions
        FlattenHelper(aggEx.GetInnerExceptions(), result);
      }
      catch (...)
      {
        // Not an AggregateException, add to result
        result.push_back(exPtr);
      }
    }
  }

  // Constructors
  AggregateException::AggregateException()
    : std::runtime_error("One or more errors occurred.")
    , m_innerExceptions()
  {
  }

  AggregateException::AggregateException(std::vector<std::exception_ptr> innerExceptions)
    : std::runtime_error(GenerateMessage("", innerExceptions))
    , m_innerExceptions(std::move(innerExceptions))
  {
    ValidateNonEmpty(m_innerExceptions);
  }

  AggregateException::AggregateException(const std::string& message, std::vector<std::exception_ptr> innerExceptions)
    : std::runtime_error(GenerateMessage(message, innerExceptions))
    , m_innerExceptions(std::move(innerExceptions))
  {
    ValidateNonEmpty(m_innerExceptions);
  }

  AggregateException::AggregateException(std::initializer_list<std::exception_ptr> innerExceptions)
    : std::runtime_error(GenerateMessage("", std::vector<std::exception_ptr>(innerExceptions)))
    , m_innerExceptions(innerExceptions)
  {
    ValidateNonEmpty(m_innerExceptions);
  }

  AggregateException::AggregateException(const std::string& message, std::initializer_list<std::exception_ptr> innerExceptions)
    : std::runtime_error(GenerateMessage(message, std::vector<std::exception_ptr>(innerExceptions)))
    , m_innerExceptions(innerExceptions)
  {
    ValidateNonEmpty(m_innerExceptions);
  }

  // Public methods
  const std::vector<std::exception_ptr>& AggregateException::GetInnerExceptions() const noexcept
  {
    return m_innerExceptions;
  }

  size_t AggregateException::InnerExceptionCount() const noexcept
  {
    return m_innerExceptions.size();
  }

  std::exception_ptr AggregateException::GetBaseException() const noexcept
  {
    if (m_innerExceptions.empty())
    {
      return nullptr;
    }
    return m_innerExceptions.front();
  }

  AggregateException AggregateException::Flatten() const
  {
    std::vector<std::exception_ptr> flattened;
    FlattenHelper(m_innerExceptions, flattened);
    return AggregateException(std::move(flattened));
  }

  std::string AggregateException::ToString() const
  {
    std::ostringstream oss;
    oss << what() << "\n";

    for (size_t i = 0; i < m_innerExceptions.size(); ++i)
    {
      oss << "  [" << i << "] ";
      try
      {
        if (m_innerExceptions[i])
        {
          std::rethrow_exception(m_innerExceptions[i]);
        }
        else
        {
          oss << "(null exception)";
        }
      }
      catch (const std::exception& ex)
      {
        oss << typeid(ex).name() << ": " << ex.what();
      }
      catch (...)
      {
        oss << "(unknown exception type)";
      }
      if (i < m_innerExceptions.size() - 1)
      {
        oss << "\n";
      }
    }

    return oss.str();
  }

  std::vector<std::exception_ptr>::const_iterator AggregateException::begin() const noexcept
  {
    return m_innerExceptions.begin();
  }

  std::vector<std::exception_ptr>::const_iterator AggregateException::end() const noexcept
  {
    return m_innerExceptions.end();
  }
}
