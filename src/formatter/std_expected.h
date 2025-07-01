//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include "formatter.h"

#include <expected>
#include <format>
#include <string>

namespace std {

template<typename T, typename E>
struct formatter<expected<T, E>, char> : formatter<std::string, char>
{
    auto format(const expected<T, E> &exp, format_context &ctx) const
    {
        if (exp.has_value()) {
            return std::format_to(ctx.out(), "{}", *exp);
        }

        return std::format_to(ctx.out(), exp.error(), *exp);
    }
};

template<typename T, typename E>
struct formatter<expected<T, E>, wchar_t> : formatter<std::wstring_view, wchar_t>
{
    auto format(const expected<T, E> &exp, wformat_context &ctx) const
    {
        if (exp.has_value()) {
            return std::format_to(ctx.out(), L"{}", *exp);
        }

        return std::format_to(ctx.out(), exp.error(), *exp);
    }
};

} // namespace std
