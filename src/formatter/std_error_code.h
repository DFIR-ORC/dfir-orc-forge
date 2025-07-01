//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include "formatter.h"

#include <format>
#include <string>
#include <system_error>

namespace std {

template<>
struct formatter<error_code, char> : formatter<std::string, char>
{
    auto format(const error_code &err, format_context &ctx) const
    {
        return formatter<std::string, char>::format(err.message(), ctx);
    }
};

//template<>
//struct formatter<error_code, wchar_t> : formatter<std::wstring, wchar_t>
//{
//    auto format(const error_code &err, wformat_context &ctx) const
//    {
//        return formatter<std::wstring, wchar_t>::format(err.message(), ctx);
//    }
//};

} // namespace std
