//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "resourceinfo.h"

#include <format>

#include "resource/resourcedirectory.h"

std::wstring ResourceInfo::GetResourceId(
    const std::wstring &type, const std::wstring &name, uint16_t langId)
{
    return std::format(L"{}/{}/{}", type, name, langId);
}

ResourceInfo::ResourceInfo()
    : lang_(0)
{}

ResourceInfo::ResourceInfo(const ResourceLangDirectory &langDirectory)
    : type_(langDirectory.nameDirectory.typeDirectory.name)
    , name_(langDirectory.nameDirectory.name)
    , lang_(langDirectory.langId)
    , id_(GetResourceId(type_, name_, lang_))
    , friendlyName_(ResourceDirectory::GetFriendlyName(langDirectory))
{}

const std::wstring &ResourceInfo::Id() const
{
    return id_;
}

const std::wstring &ResourceInfo::FriendlyName() const
{
    return friendlyName_;
}

const std::wstring &ResourceInfo::Type() const
{
    return type_;
}

const std::wstring &ResourceInfo::Name() const
{
    return name_;
}

uint16_t ResourceInfo::LangId() const
{
    return lang_;
}
