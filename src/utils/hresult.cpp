//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "hresult.h"

#include <system_error>

#include <windows.h>

const char *HRESULTErrorCategory::name() const noexcept
{
    return "HRESULT";
}

std::string HRESULTErrorCategory::message(int condition) const
{
    char buffer[2048];
    DWORD result = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        condition,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer,
        sizeof(buffer),
        nullptr);

    if (result == 0) {
        return "Unknown error";
    }

    return buffer;
}

std::error_code make_error_code_from_hr(long hr)
{
    return std::error_code(hr, hresultErrorCategory);
}
