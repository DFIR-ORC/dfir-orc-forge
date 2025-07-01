//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <format>
#include <string>
#include <QString>

namespace std {
template<>
struct formatter<QString, char> : formatter<std::string, char>
{
    auto format(const QString &qstr, format_context &ctx) const
    {
        return formatter<std::string, char>::format(qstr.toStdString(), ctx);
    }
};

template<>
struct formatter<QString, wchar_t> : formatter<std::wstring, wchar_t>
{
    auto format(const QString &qstr, wformat_context &ctx) const
    {
        return formatter<std::wstring, wchar_t>::format(qstr.toStdWString(), ctx);
    }
};
} // namespace std
