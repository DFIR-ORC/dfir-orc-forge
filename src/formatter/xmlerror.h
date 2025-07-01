//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include "utils/xmlerror.h"

namespace std {
template<>
struct formatter<XmlError, char> : formatter<std::string, char>
{
    auto format(const XmlError &err, format_context &ctx) const
    {
        return formatter<std::string, char>::format(err.toString().toStdString(), ctx);
    }
};

template<>
struct formatter<XmlError, wchar_t> : formatter<std::wstring, wchar_t>
{
    auto format(const XmlError &err, wformat_context &ctx) const
    {
        return formatter<std::wstring, wchar_t>::format(err.toString().toStdWString(), ctx);
    }
};
} // namespace std
