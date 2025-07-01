//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "xsdstore.h"

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

} // namespace

XsdStore::XsdStore()
{
    HRESULT hr = schemaCollection_
                     .CreateInstance(__uuidof(XMLSchemaCache60), nullptr, CLSCTX_INPROC_SERVER);
    if (FAILED(hr)) {
        LOG_WARN("Failed to create schema cache");
    }
}

XsdStore::~XsdStore() {}

// TODO: remove and make new instance instead
void XsdStore::Clear()
{
    xsdMap_.clear();
    HRESULT hr = schemaCollection_
                     .CreateInstance(__uuidof(XMLSchemaCache60), nullptr, CLSCTX_INPROC_SERVER);
    if (FAILED(hr)) {
        LOG_WARN("Failed to create schema cache");
    }
}

void XsdStore::loadFromDirectory(const QString &path)
{
    Clear();

    QDir directory(path);
    QStringList xsdFiles = directory.entryList(QStringList() << "*.xsd", QDir::Files);

    XmlDom commonDom;
    QString commonXsdFileName = "common.xsd";
    auto document = commonDom.loadFromFile(directory.filePath(commonXsdFileName));
    if (!document || !commonDom.get().has_value()) {
        LOG_WARN("Failed to parse file (path: {}) [{}]", commonXsdFileName, document.error());
        return;
    }

    if (schemaCollection_) {
        HRESULT hr = schemaCollection_->add(bstr_t(L""), variant_t(*commonDom.detach()));
        if (FAILED(hr)) {
            LOG_WARN("Failed to cache schema (input: common.xsd)");
        }
    }

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

    XmlDom commonDom;
    auto commonXsd = Resource::GetResourceData(L"RT_RCDATA", L"ISCHEMA_COMMON", 0);
    if (commonXsd) {
        auto common = commonDom.loadFromBuffer(QString::fromUtf8(*commonXsd));
        if (!common || !commonDom.get().has_value()) {
            LOG_WARN("Failed to parse file (path: common.xsd) [{}]", common.error());
            return;
        }

        if (schemaCollection_) {
            HRESULT hr = schemaCollection_->add(bstr_t(L""), variant_t(*commonDom.detach()));
            if (FAILED(hr)) {
                LOG_WARN("Failed to cache schema (path: common.xsd)");
            }
        }
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

MSXML::IXMLDOMSchemaCollection2Ptr XsdStore::getSchemaCache()
{
    return schemaCollection_;
}

const MSXML::IXMLDOMSchemaCollection2Ptr XsdStore::getSchemaCache() const
{
    return schemaCollection_;
}

void XsdStore::Insert(const std::wstring &inputName, XmlDom &&dom)
{
    auto document = dom.get();
    if (!document) {
        return;
    }

    if (schemaCollection_) {
        HRESULT hr = schemaCollection_->add(bstr_t(L""), variant_t(*dom.get()));
        if (FAILED(hr)) {
            LOG_WARN(L"Failed to cache schema (path: {})", inputName);
        }
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
