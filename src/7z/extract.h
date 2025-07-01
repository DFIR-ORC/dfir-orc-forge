//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <expected>
#include <string>
#include <system_error>

namespace SevenZip {

std::expected<void, std::error_code> ExtractArchive(
    const std::string_view archive, const std::wstring &outputDir);

std::expected<void, std::error_code> Extract7z(
    const std::string_view archive, const std::wstring &outputDir);

std::expected<void, std::error_code> ExtractZip(
    const std::string_view archive, const std::wstring &outputDir);

std::expected<void, std::error_code> ExtractArchive(
    const std::wstring &archive, const std::wstring &outputDir);

std::expected<void, std::error_code> Extract7z(
    const std::wstring &archive, const std::wstring &outputDir);

std::expected<void, std::error_code> ExtractZip(
    const std::wstring &archive, const std::wstring &outputDir);

} // namespace SevenZip
