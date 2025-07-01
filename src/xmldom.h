//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

// Use MSXML2 smart pointers from #import
#import "msxml6.dll" rename_namespace("MSXML")

#include <expected>
#include <QString>

#include "utils/guard.h"
#include "utils/xmlerror.h"

class XsdModel;

class XmlDom final
{
public:
    XmlDom();
    ~XmlDom();

    std::expected<MSXML::IXMLDOMDocument3Ptr, XmlError> loadFromFile(const QString &path);
    std::expected<MSXML::IXMLDOMDocument3Ptr, XmlError> loadFromBuffer(const QString &content);

    std::expected<MSXML::IXMLDOMDocument3Ptr, std::string> get() const;
    std::expected<MSXML::IXMLDOMDocument3Ptr, std::string> detach();

    std::expected<MSXML::IXMLDOMElementPtr, std::error_code> getRootElement();

    std::expected<MSXML::IXMLDOMNodePtr, std::error_code> selectSingleNode(
        const QString &xpath) const;
    std::expected<MSXML::IXMLDOMNodeListPtr, std::error_code> selectNodes(const QString &xpath);

private:
    MSXML::IXMLDOMDocument3Ptr xml_;
};
