#ifndef SERVICE_FRAMEWORK_TEST2_FRAMEWORK_CONFIG_THREADGROUPCONFIG_HPP
#define SERVICE_FRAMEWORK_TEST2_FRAMEWORK_CONFIG_THREADGROUPCONFIG_HPP
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

#include <Test2/Framework/Registry/ServiceThreadGroupId.hpp>

namespace Test2
{
  namespace ThreadGroupConfig
  {
    /// @brief The thread group ID representing the main/UI thread.
    /// Services assigned to this thread group will run cooperatively on the main thread
    /// using CooperativeThreadHost rather than spawning a dedicated thread.
    constexpr ServiceThreadGroupId MainThreadGroupId{0};
  }
}

#endif
