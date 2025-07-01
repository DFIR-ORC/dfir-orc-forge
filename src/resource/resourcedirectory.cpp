//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "resourcedirectory.h"

#include <format>

#include "formatter/formatter.h"
#include "utils/guard.h"
#include "utils/log.h"
#include "utils/win32.h"

using ResourceTypeDir = ResourceTypeDirectory;
using ResourceNameDir = ResourceNameDirectory;
using ResourceLangDir = ResourceLangDirectory;

namespace {

std::wstring StringFromResourceId(LPCWSTR resId)
{
    const std::wstring kNaN(L"#NaN");

    if (resId == nullptr) {
        return kNaN;
    }

    // BEWARE: did not found a size limit on resource size
    if (IS_INTRESOURCE(resId) == FALSE) {
        return resId;
    }

#pragma warning(push)
#pragma warning(disable : 4302)
    const WORD id = reinterpret_cast<size_t>(resId) & 0xFFFF;
#pragma warning(pop)

    try {
        return L"#" + std::to_wstring(id);
    } catch (const std::exception &e) {
        LOG_DEBUG("Failed to convert resource id: {}", e.what());
        return kNaN;
    }
}

BOOL CALLBACK EnumResourceTypeCb(HMODULE, LPWSTR lpType, LONG_PTR lParam)
{
    auto types = reinterpret_cast<std::vector<ResourceTypeDir> *>(lParam);
    auto type = StringFromResourceId(lpType);
    types->push_back(std::move(type));
    return TRUE;
}

BOOL GetResourceTypes(HMODULE hModule, std::vector<ResourceTypeDir> &types)
{
    return EnumResourceTypesW(hModule, &EnumResourceTypeCb, reinterpret_cast<LONG_PTR>(&types));
}

BOOL CALLBACK EnumResourceNamesCb(HMODULE, LPCWSTR type, LPWSTR lpName, LONG_PTR lParam)
{
    auto typeDir = reinterpret_cast<ResourceTypeDir *>(lParam);

    auto name = StringFromResourceId(lpName);
    typeDir->nameDirectories.emplace_back(std::move(name), *typeDir);
    return TRUE;
}

BOOL GetResourceNames(HMODULE hModule, const std::wstring &type, ResourceTypeDir &typeDir)
{
    return EnumResourceNamesW(
        hModule, type.c_str(), &EnumResourceNamesCb, reinterpret_cast<LONG_PTR>(&typeDir));
}

BOOL CALLBACK
EnumResourceLanguageCb(HMODULE, LPCWSTR type, LPCWSTR name, WORD wLang, LONG_PTR lParam)
{
    auto nameDir = reinterpret_cast<ResourceNameDir *>(lParam);
    nameDir->langDirectories.emplace_back(wLang, *nameDir);
    return TRUE;
}

BOOL GetResourceLang(
    HMODULE hModule, const std::wstring &type, const std::wstring &name, ResourceNameDir &nameDir)
{
    return EnumResourceLanguagesW(
        hModule,
        type.c_str(),
        name.c_str(),
        &EnumResourceLanguageCb,
        reinterpret_cast<LONG_PTR>(&nameDir));
}

} // namespace

std::wstring ResourceDirectory::GetFriendlyType(const std::wstring &type)
{
    using ResourceTypeDir = std::map<std::wstring, std::wstring>;

    const ResourceTypeDir ResourceTypeDirs
        = {{L"#1", L"RT_CURSOR"},      {L"#2", L"RT_BITMAP"},        {L"#3", L"RT_ICON"},
           {L"#4", L"RT_MENU"},        {L"#5", L"RT_DIALOG"},        {L"#6", L"RT_STRING"},
           {L"#7", L"RT_FONTDIR"},     {L"#8", L"RT_FONT"},          {L"#9", L"RT_ACCELERATOR"},
           {L"#10", L"RT_RCDATA"},     {L"#11", L"RT_MESSAGETABLE"}, {L"#12", L"RT_GROUP_CURSOR"},
           {L"#14", L"RT_GROUP_ICON"}, {L"#16", L"RT_VERSION"},      {L"#17", L"RT_DLGINCLUDE"},
           {L"#19", L"RT_PLUGPLAY"},   {L"#20", L"RT_VXD"},          {L"#21", L"RT_ANICURSOR"},
           {L"#22", L"RT_ANIICON"},    {L"#23", L"RT_HTML"},         {L"#24", L"RT_MANIFEST"}};

    ResourceTypeDir::const_iterator it;
    if ((it = ResourceTypeDirs.find(type)) == std::cend(ResourceTypeDirs)) {
        return type;
    }

    return it->second;
}

std::wstring ResourceDirectory::GetFriendlyName(const ResourceLangDirectory &langDirectory)
{
    if (langDirectory.nameDirectory.typeDirectory.name == L"BINARY") {
        if (langDirectory.nameDirectory.langDirectories.size() != 1) {
            return std::format(L"{}/{}", langDirectory.nameDirectory.name, langDirectory.langId);
        }

        return std::format(L"{}", langDirectory.nameDirectory.name);
    } else {
        auto type = GetFriendlyType(langDirectory.nameDirectory.typeDirectory.name);

        if (langDirectory.nameDirectory.langDirectories.size() != 1) {
            return std::format(
                L"{}/{}/{}", type, langDirectory.nameDirectory.name, langDirectory.langId);
        }

        return std::format(L"{}/{}", type, langDirectory.nameDirectory.name);
    }
}

ResourceDirectory::ResourceDirectory() {}

bool ResourceDirectory::loadFromFile(const std::wstring &filePath)
{
    if (!typeDirectories_.empty()) {
        LOG_CRITICAL(L"Resources already loaded (path: {})", path_);
        return false;
    }

    path_ = filePath;

    auto path = filePath;
    Guard::Module module = LoadLibraryExW(path.c_str(), NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (module == nullptr) {
        auto ec = make_error_code_from_win32(::GetLastError());
        LOG_DEBUG(
            L"Failed to get resource (path: {}) [{}]",
            path,
            QString::fromUtf8(ec.message().c_str()).toStdWString());
        return false;
    }

    if (!loadFromModule(module.value())) {
        return false;
    }

    return true;
}

bool ResourceDirectory::loadFromModule(void *moduleHandle)
{
    auto module = reinterpret_cast<HMODULE>(moduleHandle);

    BOOL ret = ::GetResourceTypes(module, typeDirectories_);
    if (ret == FALSE) {
        LOG_WARN("Failed enumerating rsrc types [{}]", GetLastError());
    }

    for (auto &typeDir : typeDirectories_) {
        const auto &type = typeDir.name;
        ret = ::GetResourceNames(module, typeDir.name, typeDir);
        if (ret == FALSE) {
            LOG_WARN("Failed enumerating rsrc name [{}]", GetLastError());
        }

        for (auto &nameDir : typeDir.nameDirectories) {
            const auto &name = nameDir.name;
            ret = ::GetResourceLang(module, type, name, nameDir);
            if (ret == FALSE) {
                LOG_WARN("Failed enumerating rsrc lang [{}]", GetLastError());
            }
        }

        typeDir.resources = this;
    }

    return true;
}

const std::vector<ResourceTypeDirectory> &ResourceDirectory::TypeDirectories() const
{
    return typeDirectories_;
}

const std::vector<ResourceNameDirectory> &ResourceDirectory::NameDirectories(
    const ResourceTypeDirectory &type) const
{
    return type.nameDirectories;
}

const std::vector<ResourceLangDirectory> &ResourceDirectory::LangDirectories(
    const ResourceNameDirectory &name) const
{
    return name.langDirectories;
}

std::optional<std::wstring> ResourceDirectory::Path() const
{
    return path_;
}
