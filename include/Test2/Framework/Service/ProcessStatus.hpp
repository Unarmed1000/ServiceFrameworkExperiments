#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_PROCESSSTATUS_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_SERVICE_PROCESSSTATUS_HPP
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

namespace Test2
{
  /// @brief Represents the status result of a service Process() call.
  enum class ProcessStatus
  {
    /// @brief This process did not result in anything that would limit the sleep time.
    NoSleepLimit = 0,

    /// @brief Duration contains the recommended max time the thread should sleep.
    ///        This means it's ok for it to be called sooner, but it should not be called much later.
    SleepLimit = 1,

    /// @brief The process operation would like to quit.
    Quit = 2
  };
}

#endif
