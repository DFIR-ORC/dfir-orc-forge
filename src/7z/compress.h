//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

void Compress7z(
    const std::filesystem::path &inputFile, std::vector<uint8_t> &out, std::error_code &ec);

void Compress7z(
    std::string_view content,
    const std::wstring &fileNameInArchive,
    std::vector<uint8_t> &out,
    std::error_code &ec);

void Compress7z(
    const std::vector<uint8_t> &content,
    const std::wstring &fileNameInArchive,
    std::vector<uint8_t> &out,
    std::error_code &ec);
