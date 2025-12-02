#ifndef SERVICE_FRAMEWORK_COMMON_AGGREGATEEXCEPTION_HPP
#define SERVICE_FRAMEWORK_COMMON_AGGREGATEEXCEPTION_HPP
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

#include <exception>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

namespace Common
{
  /// @brief Represents one or more errors that occur during application execution.
  ///
  /// This exception aggregates multiple exceptions similar to C#'s System.AggregateException.
  /// Once constructed, the exception is immutable and thread-safe for reading.
  /// All inner exceptions are stored as std::exception_ptr for type-erased storage.
  class AggregateException : public std::runtime_error
  {
  private:
    std::vector<std::exception_ptr> m_innerExceptions;

    /// @brief Generates the default message for the exception.
    /// @param message Custom message or empty for default.
    /// @param exceptions The inner exceptions.
    /// @return The formatted message string.
    static std::string GenerateMessage(const std::string& message, const std::vector<std::exception_ptr>& /*exceptions*/)
    {
      if (!message.empty())
      {
        return message;
      }
      return "One or more errors occurred.";
    }

    /// @brief Validates that the exception vector is not empty.
    /// @param exceptions The exceptions to validate.
    /// @throws std::invalid_argument if the vector is empty.
    static void ValidateNonEmpty(const std::vector<std::exception_ptr>& exceptions)
    {
      if (exceptions.empty())
      {
        throw std::invalid_argument("The innerExceptions argument must contain at least one exception.");
      }
    }

  public:
    /// @brief Default constructor that creates an AggregateException with no inner exceptions.
    AggregateException()
      : std::runtime_error("One or more errors occurred.")
      , m_innerExceptions()
    {
    }

    /// @brief Initializes a new instance of the AggregateException class with a collection of exception pointers.
    /// @param innerExceptions The exceptions that are the cause of the current exception.
    /// @throws std::invalid_argument if innerExceptions is empty.
    explicit AggregateException(std::vector<std::exception_ptr> innerExceptions)
      : std::runtime_error(GenerateMessage("", innerExceptions))
      , m_innerExceptions(std::move(innerExceptions))
    {
      ValidateNonEmpty(m_innerExceptions);
    }

    /// @brief Initializes a new instance of the AggregateException class with a custom message and a collection of exception pointers.
    /// @param message The error message that explains the reason for the exception.
    /// @param innerExceptions The exceptions that are the cause of the current exception.
    /// @throws std::invalid_argument if innerExceptions is empty.
    AggregateException(const std::string& message, std::vector<std::exception_ptr> innerExceptions)
      : std::runtime_error(GenerateMessage(message, innerExceptions))
      , m_innerExceptions(std::move(innerExceptions))
    {
      ValidateNonEmpty(m_innerExceptions);
    }

    /// @brief Initializes a new instance of the AggregateException class with an initializer list of exception pointers.
    /// @param innerExceptions The exceptions that are the cause of the current exception.
    /// @throws std::invalid_argument if innerExceptions is empty.
    AggregateException(std::initializer_list<std::exception_ptr> innerExceptions)
      : std::runtime_error(GenerateMessage("", std::vector<std::exception_ptr>(innerExceptions)))
      , m_innerExceptions(innerExceptions)
    {
      ValidateNonEmpty(m_innerExceptions);
    }

    /// @brief Initializes a new instance of the AggregateException class with a custom message and an initializer list of exception pointers.
    /// @param message The error message that explains the reason for the exception.
    /// @param innerExceptions The exceptions that are the cause of the current exception.
    /// @throws std::invalid_argument if innerExceptions is empty.
    AggregateException(const std::string& message, std::initializer_list<std::exception_ptr> innerExceptions)
      : std::runtime_error(GenerateMessage(message, std::vector<std::exception_ptr>(innerExceptions)))
      , m_innerExceptions(innerExceptions)
    {
      ValidateNonEmpty(m_innerExceptions);
    }

    /// @brief Template constructor that accepts a container of any exception types.
    /// @tparam TContainer A container type (e.g., std::vector, std::list) of exception objects.
    /// @param exceptions The exceptions that are the cause of the current exception.
    /// @throws std::invalid_argument if exceptions is empty.
    template <typename TContainer>
    explicit AggregateException(const TContainer& exceptions,
                                typename std::enable_if<!std::is_same<TContainer, std::vector<std::exception_ptr>>::value &&
                                                        !std::is_same<TContainer, std::string>::value>::type* = nullptr)
      : std::runtime_error("One or more errors occurred.")
      , m_innerExceptions(ConvertToExceptionPtrs(exceptions))
    {
      ValidateNonEmpty(m_innerExceptions);
    }

    /// @brief Template constructor that accepts a custom message and a container of any exception types.
    /// @tparam TContainer A container type (e.g., std::vector, std::list) of exception objects.
    /// @param message The error message that explains the reason for the exception.
    /// @param exceptions The exceptions that are the cause of the current exception.
    /// @throws std::invalid_argument if exceptions is empty.
    template <typename TContainer>
    AggregateException(const std::string& message, const TContainer& exceptions,
                       typename std::enable_if<!std::is_same<TContainer, std::vector<std::exception_ptr>>::value>::type* = nullptr)
      : std::runtime_error(message.empty() ? "One or more errors occurred." : message)
      , m_innerExceptions(ConvertToExceptionPtrs(exceptions))
    {
      ValidateNonEmpty(m_innerExceptions);
    }

    // Copy and move constructors are allowed, but assignment operators are deleted to maintain immutability
    AggregateException(const AggregateException&) = default;
    AggregateException(AggregateException&&) = default;
    AggregateException& operator=(const AggregateException&) = delete;
    AggregateException& operator=(AggregateException&&) = delete;

    /// @brief Gets a read-only collection of the inner exceptions.
    /// @return A const reference to the vector of exception pointers.
    const std::vector<std::exception_ptr>& GetInnerExceptions() const noexcept
    {
      return m_innerExceptions;
    }

    /// @brief Gets the number of inner exceptions.
    /// @return The count of inner exceptions.
    size_t InnerExceptionCount() const noexcept
    {
      return m_innerExceptions.size();
    }

    /// @brief Gets the first inner exception from the innermost AggregateException.
    /// @return An exception_ptr to the base exception.
    std::exception_ptr GetBaseException() const noexcept
    {
      if (m_innerExceptions.empty())
      {
        return nullptr;
      }
      return m_innerExceptions.front();
    }

    /// @brief Flattens the AggregateException hierarchy into a single AggregateException.
    ///
    /// This method recursively unwraps all nested AggregateException instances and returns
    /// a new AggregateException containing all the leaf exceptions.
    /// @return A new AggregateException with all nested exceptions flattened.
    AggregateException Flatten() const
    {
      std::vector<std::exception_ptr> flattened;
      FlattenHelper(m_innerExceptions, flattened);
      return AggregateException(std::move(flattened));
    }

    /// @brief Returns a detailed string representation of the exception and all inner exceptions.
    /// @return A formatted string with exception details.
    std::string ToString() const
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

    /// @brief Returns an iterator to the beginning of the inner exceptions collection.
    /// @return A const iterator to the first element.
    std::vector<std::exception_ptr>::const_iterator begin() const noexcept
    {
      return m_innerExceptions.begin();
    }

    /// @brief Returns an iterator to the end of the inner exceptions collection.
    /// @return A const iterator to the element following the last element.
    std::vector<std::exception_ptr>::const_iterator end() const noexcept
    {
      return m_innerExceptions.end();
    }

  private:
    /// @brief Helper function to recursively flatten nested AggregateExceptions.
    /// @param exceptions The exceptions to flatten.
    /// @param result The output vector for flattened exceptions.
    static void FlattenHelper(const std::vector<std::exception_ptr>& exceptions, std::vector<std::exception_ptr>& result)
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

    /// @brief Converts a container of exception objects to a vector of exception_ptr.
    /// @tparam TContainer A container type of exception objects.
    /// @param exceptions The container of exceptions to convert.
    /// @return A vector of exception_ptr.
    template <typename TContainer>
    static std::vector<std::exception_ptr> ConvertToExceptionPtrs(const TContainer& exceptions)
    {
      std::vector<std::exception_ptr> result;
      result.reserve(std::distance(std::begin(exceptions), std::end(exceptions)));

      for (const auto& ex : exceptions)
      {
        try
        {
          throw ex;
        }
        catch (...)
        {
          result.push_back(std::current_exception());
        }
      }

      return result;
    }
  };
}

#endif
