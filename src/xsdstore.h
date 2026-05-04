//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#import "msxml6.dll" rename_namespace("MSXML")

#include <map>

#include <expected>
#include <string>

#include "utils/guard.h"
#include "utils/xmlerror.h"
#include "xmldom.h"

// TODO: remove ?
class XsdStore
{
public:
    XsdStore();
    ~XsdStore();

    void loadFromDirectory(const QString &path);
    void loadFromResources();

    std::expected<MSXML::IXMLDOMDocument3Ptr, std::string> get(const QString &identifier) const;
    MSXML::IXMLDOMSchemaCollection2Ptr getNormalizedSchema();
    MSXML::IXMLDOMDocument3Ptr getCommonDocument() const;

    void Clear();

private:
    void Insert(const std::wstring &inputName, XmlDom &&dom);

private:
    std::map<QString, XmlDom> xsdMap_;
    MSXML::IXMLDOMSchemaCollection2Ptr schemaCollection_;
    MSXML::IXMLDOMDocument3Ptr commonDocument_;
};
