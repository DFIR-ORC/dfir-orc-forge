//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include "resourceinfo.h"

#include <expected>
#include <system_error>
#include <vector>

namespace Guard {
class Module;
}

class Resource : public ResourceInfo
{
public:
    static std::expected<std::vector<uint8_t>, std::error_code> GetResourceData(
        const std::wstring &path, const std::wstring &type, const std::wstring &name, uint16_t lang);

    static std::expected<std::vector<uint8_t>, std::error_code> GetResourceData(
        const std::wstring &path, const ResourceInfo &info);

    static std::expected<std::vector<uint8_t>, std::error_code> GetResourceData(
        Guard::Module &module, const std::wstring &type, const std::wstring &name, uint16_t lang);

    static std::expected<std::vector<uint8_t>, std::error_code> GetResourceData(
        Guard::Module &module, const ResourceInfo &info);

    static std::expected<std::vector<uint8_t>, std::error_code> GetResourceData(
        const std::wstring &type, const std::wstring &name, uint16_t lang);

    static std::expected<void, std::error_code> UpdateResources(
        const std::wstring &path, std::vector<Resource> &items);

public:
    Resource(ResourceInfo info, std::vector<uint8_t> data);

public:
    std::vector<uint8_t> &Data();

private:
    std::vector<uint8_t> data_;
};
