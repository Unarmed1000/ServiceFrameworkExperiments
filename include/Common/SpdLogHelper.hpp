#ifndef SERVICE_FRAMEWORK_COMMON_SPDLOGHELPER_HPP
#define SERVICE_FRAMEWORK_COMMON_SPDLOGHELPER_HPP
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

#include <spdlog/spdlog.h>
#include <memory>
#include <string_view>

namespace SpdLogHelper
{
  /// @brief Gets or creates a logger with the specified name, inheriting global sink configuration.
  /// @tparam Name The compile-time string for the logger name.
  /// @return Shared pointer to the logger.
  template <typename Name>
  inline std::shared_ptr<spdlog::logger> GetLogger()
  {
    static const auto logger = []()
    {
      constexpr std::string_view name = Name::value;
      auto log = spdlog::get(std::string(name));
      if (!log)
      {
        // Use default logger's sinks - inherits global configuration
        log = std::make_shared<spdlog::logger>(std::string(name), spdlog::default_logger()->sinks());
        spdlog::register_logger(log);
      }
      return log;
    }();
    return logger;
  }

  /// @brief Gets or creates a logger with the specified name, inheriting global sink configuration.
  /// @param name The logger name.
  /// @return Shared pointer to the logger.
  inline std::shared_ptr<spdlog::logger> GetLogger(const std::string& name)
  {
    auto log = spdlog::get(name);
    if (!log)
    {
      // Use default logger's sinks - inherits global configuration
      log = std::make_shared<spdlog::logger>(name, spdlog::default_logger()->sinks());
      spdlog::register_logger(log);
    }
    return log;
  }
}

// Macro to define a compile-time string type for logger names
#define LOGGER_NAME(name)                            \
  struct LoggerName_##name                           \
  {                                                  \
    static constexpr std::string_view value = #name; \
  }

#endif
