//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "resource.h"

#include <format>
#include <string.h>
#include <string_view>
#include <system_error>

#include "utils/guard.h"
#include "utils/log.h"
#include "utils/win32.h"

namespace {

struct RtAlias
{
    const wchar_t *name; // without the "RT_" prefix
    WORD id;
};

// The standard RT_ resource types. Mirrors the Win32 RT_* constants and the
// table in resourcedirectory.cpp.
constexpr RtAlias kRtAliases[] = {
    {L"CURSOR", 1},       {L"BITMAP", 2},   {L"ICON", 3},         {L"MENU", 4},
    {L"DIALOG", 5},       {L"STRING", 6},   {L"FONTDIR", 7},      {L"FONT", 8},
    {L"ACCELERATOR", 9},  {L"RCDATA", 10},  {L"MESSAGETABLE", 11},{L"GROUP_CURSOR", 12},
    {L"GROUP_ICON", 14},  {L"VERSION", 16}, {L"DLGINCLUDE", 17},  {L"PLUGPLAY", 19},
    {L"VXD", 20},         {L"ANICURSOR", 21},{L"ANIICON", 22},    {L"HTML", 23},
    {L"MANIFEST", 24},
};

// Resolves a resource-type string to the pointer FindResource/UpdateResource
// expect. An integer resource type may be written as its RT_ alias
// ("RT_RCDATA") or in the "#N" form the resource enumerator produces; both
// resolve to MAKEINTRESOURCE(N). Any other string is a genuine user-defined
// type name and is returned unchanged -- the pointer then aliases 'type', which
// must outlive the returned value.
LPCWSTR ResolveResourceType(const std::wstring &type)
{
    // "#N" numeric form.
    if (type.size() > 1 && type.front() == L'#') {
        unsigned long id = 0;
        bool digits = true;
        for (size_t i = 1; i < type.size(); ++i) {
            if (type[i] < L'0' || type[i] > L'9') {
                digits = false;
                break;
            }
            id = id * 10 + static_cast<unsigned long>(type[i] - L'0');
            if (id > 0xFFFF) {
                digits = false;
                break;
            }
        }
        if (digits && id > 0)
            return MAKEINTRESOURCEW(static_cast<WORD>(id));
    }

    // Standard RT_ alias (with or without the "RT_" prefix).
    std::wstring_view name = type;
    if (name.size() > 3 && _wcsnicmp(name.data(), L"RT_", 3) == 0)
        name.remove_prefix(3);
    for (const auto &alias : kRtAliases) {
        if (_wcsicmp(std::wstring(name).c_str(), alias.name) == 0)
            return MAKEINTRESOURCEW(alias.id);
    }

    // Genuine string type name.
    return type.c_str();
}

} // namespace

std::expected<std::vector<uint8_t>, std::error_code> Resource::GetResourceData(
    const std::wstring &type, const std::wstring &name, uint16_t lang)
{
    Guard::Module module;
    return GetResourceData(module, type, name, lang);
}

std::expected<std::vector<uint8_t>, std::error_code> Resource::GetResourceData(
    Guard::Module &module, const std::wstring &type, const std::wstring &name, uint16_t lang)
{
    std::error_code ec;

    HRSRC hResInfo = FindResourceExW(module.value(), ResolveResourceType(type), name.c_str(), lang);
    if (hResInfo == nullptr) {
        ec.assign(GetLastError(), std::system_category());
        LOG_CRITICAL("Failed to find resource: {}", ec.message());
        return std::unexpected(ec);
    }

    HGLOBAL hResData = LoadResource(module.value(), hResInfo);
    if (hResData == nullptr) {
        ec.assign(GetLastError(), std::system_category());
        LOG_CRITICAL("Failed to load resource: {}", ec.message());
        return std::unexpected(ec);
    }

    LPVOID resource = LockResource(hResData);
    if (resource == nullptr) {
        ec.assign(GetLastError(), std::system_category());
        LOG_CRITICAL("Failed to lock resource: {}", ec.message());
        return std::unexpected(ec);
    }

    DWORD resourceSize = SizeofResource(module.value(), hResInfo);
    if (resourceSize == 0) {
        ec.assign(GetLastError(), std::system_category());
        LOG_CRITICAL("Failed to get resource size: {}", ec.message());
        return std::unexpected(ec);
    }

    std::string_view view(reinterpret_cast<const char *>(resource), resourceSize);

    std::vector<uint8_t> out;
    std::copy(std::cbegin(view), std::cend(view), std::back_inserter(out));
    return out;
}

std::expected<std::vector<uint8_t>, std::error_code> Resource::GetResourceData(
    Guard::Module &module, const ResourceInfo &info)
{
    return GetResourceData(module, info.Type(), info.Name(), info.LangId());
}

std::expected<void, std::error_code> Resource::UpdateResources(
    const std::wstring &path, std::vector<Resource> &items)
{
    Guard::BeginUpdateResourceHandleW hUpdate = BeginUpdateResourceW(path.c_str(), FALSE);
    if (!hUpdate) {
        auto ec = make_error_code_from_win32(::GetLastError());
        LOG_DEBUG(
            L"Failed to open file for resource update (path: {}) [{}]",
            path,
            QString::fromUtf8(ec.message().c_str()).toStdWString());
        return std::unexpected(ec);
    }

    for (auto &item : items) {
        BOOL ret = UpdateResourceW(
            hUpdate.value(),
            ResolveResourceType(item.Type()),
            item.Name().c_str(),
            item.LangId(),
            reinterpret_cast<void *>(item.Data().data()),
            item.Data().size());
        if (!ret) {
            auto ec = make_error_code_from_win32(::GetLastError());
            LOG_DEBUG(
                L"Failed to update resource (path: {}, id: {})[{}]",
                path,
                item.Id(),
                QString::fromUtf8(ec.message()).toStdWString());
            return std::unexpected(ec);
        }

        LOG_DEBUG(L"Added resource for update (path: {}, id: {})", path, item.Id());
    }

    BOOL ret = EndUpdateResourceW(hUpdate.detach(), FALSE);
    if (!ret) {
        auto ec = make_error_code_from_win32(::GetLastError());
        LOG_DEBUG(
            L"Failed to write file while updating resources (path: {}) [{}]",
            path,
            QString::fromUtf8(ec.message()).toStdWString());
        return std::unexpected(ec);
    }

    LOG_DEBUG(L"Update resources (path: {})", path);
    return {};
}

std::expected<std::vector<uint8_t>, std::error_code> Resource::GetResourceData(
    const std::wstring &path, const std::wstring &type, const std::wstring &name, uint16_t lang)
{
    Guard::Module module = LoadLibraryExW(path.c_str(), NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!module.IsValid()) {
        auto ec = make_error_code_from_win32(::GetLastError());
        LOG_DEBUG(
            L"Failed to get resource (path: {}) [{}]",
            path,
            QString::fromUtf8(ec.message()).toStdWString());
        return std::unexpected(ec);
    }

    return GetResourceData(module, type, name, lang);
}

std::expected<std::vector<uint8_t>, std::error_code> Resource::GetResourceData(
    const std::wstring &path, const ResourceInfo &info)
{
    return GetResourceData(path, info.Type(), info.Name(), info.LangId());
}

Resource::Resource(ResourceInfo info, std::vector<uint8_t> data)
    : ResourceInfo(std::move(info))
    , data_(data)
{}

std::vector<uint8_t> &Resource::Data()
{
    return data_;
}
