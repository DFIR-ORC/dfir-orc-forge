//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <string>

class ResourceLangDirectory;

class ResourceInfo
{
public:
    static std::wstring GetResourceId(
        const std::wstring &type, const std::wstring &name, uint16_t lang);

    ResourceInfo();
    ResourceInfo(const ResourceLangDirectory &langDirectory);
    ResourceInfo(const ResourceInfo &resource) = default;

    const std::wstring &Id() const;
    const std::wstring &FriendlyName() const;

    const std::wstring &Type() const;
    const std::wstring &Name() const;
    uint16_t LangId() const;

private:
    std::wstring type_;
    std::wstring name_;
    uint16_t lang_;
    std::wstring id_;
    std::wstring friendlyName_; // TODO: remove friendly name and process it in the ui
};
