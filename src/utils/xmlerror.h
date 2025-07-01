//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#import "msxml6.dll" rename_namespace("MSXML")

#include <optional>
#include <string>
#include <system_error>

#include <QString>

struct IXMLDOMParseError;

struct XmlErrorLocation
{
    std::wstring message;
    uint32_t line = 0;
    uint32_t column = 0;
};

struct XmlError
{
public:
    XmlError(const std::error_code &e);
    XmlError(int value, const std::error_category &category);
    XmlError(std::errc &errc);
    XmlError(IXMLDOMParseError &parseError);
    XmlError(const MSXML::IXMLDOMParseErrorPtr &parseError);

    QString toString() const;

    std::error_code error;
    std::optional<XmlErrorLocation> location;
};
