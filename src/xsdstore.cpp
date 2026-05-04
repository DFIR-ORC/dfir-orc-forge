//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "xsdstore.h"

#include <algorithm>
#include <cwchar>
#include <expected>
#include <msxml6.h>
#include <string>
#include <vector>

#include <comdef.h>

#include <QDir>
#include <QFile>
#include <QString>
#include <QTextStream>

#include "formatter/formatter.h"
#include "resource/resource.h"
#include "resource/resourcedirectory.h"
#include "utils/hresult.h"
#include "utils/log.h"

namespace {

std::expected<QString, std::error_code> GetRootElementAttributeName(MSXML::IXMLDOMDocument3Ptr xml)
{
    MSXML::IXMLDOMNodePtr node;

    HRESULT hr = xml->raw_selectSingleNode(bstr_t(L"/xs:schema/xs:element[@name]"), &node);
    if (FAILED(hr)) {
        return std::unexpected(make_error_code_from_hr(hr));
    }

    if (!node) {
        return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }

    IXMLDOMElementPtr element;
    hr = node->QueryInterface(IID_IXMLDOMElement, (void **) &element);
    if (FAILED(hr)) {
        return std::unexpected(make_error_code_from_hr(hr));
    }

    variant_t name;
    hr = element->getAttribute(bstr_t(L"name"), &name);
    if (FAILED(hr)) {
        return std::unexpected(make_error_code_from_hr(hr));
    }

    if (name.vt != VT_BSTR) {
        return std::unexpected(make_error_code_from_hr(hr));
    }

    return QString::fromWCharArray(name.bstrVal);
}

void NormalizeXsdNames(MSXML::IXMLDOMDocument3Ptr xml)
{
    const std::pair<const wchar_t *, const wchar_t *> queries[] = {
        {L"//xs:element[@name]", L"name"},
        {L"//xs:attribute[@name]", L"name"},
        {L"//xs:element[@ref]", L"ref"},
        {L"//xs:attribute[@ref]", L"ref"},
    };

    for (const auto &[xpath, attrName] : queries) {
        MSXML::IXMLDOMNodeListPtr nodes;
        HRESULT hr = xml->raw_selectNodes(bstr_t(xpath), &nodes);
        if (FAILED(hr) || !nodes) {
            continue;
        }

        long length = 0;
        nodes->get_length(&length);
        for (long i = 0; i < length; ++i) {
            MSXML::IXMLDOMNodePtr node;
            nodes->get_item(i, &node);
            if (!node) {
                continue;
            }

            IXMLDOMElementPtr element;
            hr = node->QueryInterface(IID_IXMLDOMElement, (void **) &element);
            if (FAILED(hr) || !element) {
                continue;
            }

            variant_t value;
            hr = element->getAttribute(bstr_t(attrName), &value);
            if (FAILED(hr) || value.vt != VT_BSTR) {
                continue;
            }

            std::wstring lower(value.bstrVal);
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            element->setAttribute(bstr_t(attrName), variant_t(lower.c_str()));
        }
    }
}

} // namespace

XsdStore::XsdStore() {}

XsdStore::~XsdStore() {}

// TODO: remove and make new instance instead
void XsdStore::Clear()
{
    xsdMap_.clear();
    commonDocument_ = nullptr;
    schemaCollection_ = nullptr;
}

void XsdStore::loadFromDirectory(const QString &path)
{
    Clear();

    QDir directory(path);
    QStringList xsdFiles = directory.entryList(QStringList() << "*.xsd", QDir::Files);

    QString commonXsdFileName = "common.xsd";
    XmlDom commonDom;
    auto document = commonDom.loadFromFile(directory.filePath(commonXsdFileName));
    if (!document || !commonDom.get().has_value()) {
        LOG_WARN("Failed to parse file (path: {}) [{}]", commonXsdFileName, document.error());
        return;
    }
    commonDocument_ = commonDom.get().value_or(nullptr);

    for (const QString &fileName : xsdFiles) {
        if (fileName == commonXsdFileName) {
            continue;
        }

        XmlDom dom;
        auto document = dom.loadFromFile(directory.filePath(fileName));
        if (!document || !dom.get().has_value()) {
            LOG_WARN("Failed to parse file (path: {})", fileName, document.error());
            continue;
        }
        Insert(fileName.toStdWString(), std::move(dom));
    }
}

void XsdStore::loadFromResources()
{
    Clear();

    auto commonXsd = Resource::GetResourceData(L"RT_RCDATA", L"ISCHEMA_COMMON", 0);
    if (commonXsd) {
        XmlDom commonDom;
        auto common = commonDom.loadFromBuffer(QString::fromUtf8(*commonXsd));
        if (!common || !commonDom.get().has_value()) {
            LOG_WARN("Failed to parse file (path: common.xsd) [{}]", common.error());
            return;
        }
        commonDocument_ = commonDom.get().value_or(nullptr);
    }

    ResourceDirectory resources;
    if (!resources.loadFromModule(nullptr)) {
        return;
    }

    for (const auto &typeDir : resources.TypeDirectories()) {
        // The schemas are RT_RCDATA (integer type 10); the enumerator reports
        // that as "#10", so compare the normalized friendly type name.
        if (ResourceDirectory::GetFriendlyType(typeDir.name) != L"RT_RCDATA") {
            continue;
        }

        for (const auto &nameDir : typeDir.nameDirectories) {
            if (!nameDir.name.starts_with(L"SCHEMA_")) {
                continue;
            }

            auto xsd = Resource::GetResourceData(L"RT_RCDATA", nameDir.name, 0);
            if (!xsd) {
                LOG_DEBUG(L"Failed to load form (resource name: {})", nameDir.name);
                continue;
            }

            XmlDom dom;
            auto document = dom.loadFromBuffer(QString::fromUtf8(*xsd));
            if (!document || !dom.get().has_value()) {
                LOG_WARN(L"Failed to parse file (path: {}) [{}]", nameDir.name, document.error());
                continue;
            }

            LOG_DEBUG(L"Add schema: {}", nameDir.name);
            Insert(nameDir.name, std::move(dom));
        }
    }
}

std::expected<MSXML::IXMLDOMDocument3Ptr, std::string> XsdStore::get(const QString &identifier) const
{
    auto it = xsdMap_.find(identifier);
    if (it == std::cend(xsdMap_)) {
        return std::unexpected(
            "No validation schema available for '" + identifier.toStdString() + "'");
    }

    if (it->second.get() == nullptr) {
        return std::unexpected("XSD with invalid COM pointer " + identifier.toStdString());
    }

    return it->second.get();
}

MSXML::IXMLDOMSchemaCollection2Ptr XsdStore::getNormalizedSchema()
{
    if (schemaCollection_) {
        return schemaCollection_;
    }

    HRESULT hr = schemaCollection_
                     .CreateInstance(__uuidof(XMLSchemaCache60), nullptr, CLSCTX_INPROC_SERVER);
    if (FAILED(hr)) {
        LOG_WARN("Failed to create schema cache");
        return nullptr;
    }

    auto addNormalized = [&](MSXML::IXMLDOMDocument3Ptr original) {
        if (!original) {
            return;
        }
        BSTR rawXml = nullptr;
        original->get_xml(&rawXml);
        if (!rawXml) {
            return;
        }
        _bstr_t owned(rawXml, false);
        XmlDom normalizedDom;
        auto loaded = normalizedDom.loadFromBuffer(QString::fromWCharArray(owned));
        if (!loaded) {
            return;
        }
        NormalizeXsdNames(loaded->GetInterfacePtr());
        if (auto doc = normalizedDom.detach()) {
            HRESULT addHr = schemaCollection_->add(bstr_t(L""), variant_t(*doc));
            if (FAILED(addHr)) {
                LOG_WARN("Failed to add normalized schema to cache");
            }
        }
    };

    addNormalized(commonDocument_);
    for (auto &[id, dom] : xsdMap_) {
        if (auto doc = dom.get()) {
            addNormalized(*doc);
        }
    }

    return schemaCollection_;
}

MSXML::IXMLDOMDocument3Ptr XsdStore::getCommonDocument() const
{
    return commonDocument_;
}

void XsdStore::Insert(const std::wstring &inputName, XmlDom &&dom)
{
    auto document = dom.get();
    if (!document) {
        return;
    }

    auto identifier = GetRootElementAttributeName(document->GetInterfacePtr());
    if (!identifier) {
        LOG_WARN("Failed to retrieve identifier [{}]", identifier.error());
        return;
    }

    auto [it, inserted] = xsdMap_.emplace(std::move(*identifier), std::move(dom));
    if (!inserted) {
        LOG_WARN("Already had a xsd with the same identifier (value: {})", *identifier);
        return;
    }
}
