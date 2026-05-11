//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "peinfo.h"

#include <format>
#include <vector>

#include <windows.h>
#include <winver.h>

PeProductInfo peProductInfo(const QString &exePath)
{
    const std::wstring wide = exePath.toStdWString();
    DWORD dummy = 0;
    const DWORD size = GetFileVersionInfoSizeW(wide.c_str(), &dummy);
    if (size == 0)
        return {};

    std::vector<BYTE> buf(size);
    if (!GetFileVersionInfoW(wide.c_str(), 0, size, buf.data()))
        return {};

    struct LangCodePage
    {
        WORD lang;
        WORD codePage;
    } *trans = nullptr;
    UINT transLen = 0;
    if (!VerQueryValueW(
            buf.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID *>(&trans), &transLen)
        || transLen == 0)
        return {};

    const std::wstring prefix
        = std::format(L"\\StringFileInfo\\{:04X}{:04X}\\", trans->lang, trans->codePage);

    auto queryString = [&](std::wstring_view key) -> QString {
        wchar_t *val = nullptr;
        UINT valLen = 0;
        const std::wstring path = prefix + std::wstring(key);
        if (VerQueryValueW(buf.data(), path.c_str(), reinterpret_cast<LPVOID *>(&val), &valLen)
            && valLen > 0)
            return QString::fromWCharArray(val, static_cast<int>(valLen) - 1);
        return {};
    };

    return {
        queryString(L"ProductName"),
        queryString(L"ProductVersion"),
        queryString(L"FileDescription")};
}
