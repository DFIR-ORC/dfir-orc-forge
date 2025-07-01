//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <memory>
#include <optional>
#include <system_error>
#include <vector>

namespace Guard {
class Module;
}

class ResourceInfo;
class ResourceNameDirectory;
class ResourceTypeDirectory;
class ResourceDirectory;

class ResourceLangDirectory
{
public:
    ResourceLangDirectory(uint16_t l, ResourceNameDirectory &parent)
        : langId(l)
        , nameDirectory(parent)
    {}

    uint16_t langId;
    ResourceNameDirectory &nameDirectory;
};

class ResourceNameDirectory
{
public:
    ResourceNameDirectory(const std::wstring &n, ResourceTypeDirectory &parent)
        : name(n)
        , typeDirectory(parent)
    {}

    std::wstring name;
    std::vector<ResourceLangDirectory> langDirectories;
    ResourceTypeDirectory &typeDirectory;
};

class ResourceTypeDirectory
{
public:
    ResourceTypeDirectory(const std::wstring &n)
        : name(n)
        , resources(nullptr)
    {}

    std::wstring name;
    std::vector<ResourceNameDirectory> nameDirectories;
    ResourceDirectory *resources;
};

class ResourceDirectory
{
public:
    static std::wstring GetFriendlyType(const std::wstring &type);
    static std::wstring GetFriendlyName(const ResourceLangDirectory &langDirectory);

    ResourceDirectory();

    bool loadFromFile(const std::wstring &filePath);
    bool loadFromModule(void *moduleHandle);

    const std::vector<ResourceTypeDirectory> &TypeDirectories() const;
    const std::vector<ResourceNameDirectory> &NameDirectories(
        const ResourceTypeDirectory &type) const;
    const std::vector<ResourceLangDirectory> &LangDirectories(
        const ResourceNameDirectory &name) const;

    std::optional<std::wstring> Path() const;

private:
    std::optional<std::wstring> path_;
    std::vector<ResourceTypeDirectory> typeDirectories_;
};
