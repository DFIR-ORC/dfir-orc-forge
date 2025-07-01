//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include "formatter.h"

#include <optional>

#include <format>

namespace std {

template<typename T>
struct formatter<optional<T>> : public std::formatter<std::string_view>
{
    template<typename FormatContext>
    auto format(const std::optional<T> &optional, FormatContext &ctx) const
    {
        if (!optional.has_value()) {
            return std::formatter<std::string_view>::format("<N/A>", ctx);
        }

        return std::format_to(ctx.out(), "{}", *optional);
    }
};

template<typename T>
struct formatter<optional<T>, wchar_t> : public std::formatter<std::wstring_view, wchar_t>
{
    template<typename FormatContext>
    auto format(const std::optional<T> &optional, FormatContext &ctx) const
    {
        if (!optional.has_value()) {
            return std::formatter<std::wstring_view, wchar_t>::format(L"<N/A>", ctx);
        }

        return std::format_to(ctx.out(), L"{}", *optional);
    }
};

} // namespace std
