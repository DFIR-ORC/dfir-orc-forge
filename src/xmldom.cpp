//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "xmldom.h"

#include <string>

#include <comdef.h>
#include <msxml6.h>

#include <QFileInfo>
#include <QTextStream>

#include "formatter/formatter.h"
#include "utils/hresult.h"
#include "utils/log.h"
#include "utils/qtfileerror.h"

namespace {

std::expected<MSXML::IXMLDOMDocument3Ptr, XmlError> CreateXmlDom(const QString &text)
{
    MSXML::IXMLDOMDocument3Ptr xml;

    HRESULT hr = xml.CreateInstance(__uuidof(DOMDocument60), nullptr, CLSCTX_INPROC_SERVER);
    if (FAILED(hr)) {
        LOG_CRITICAL("Failed CoCreateInstance");
        XmlError e(make_error_code_from_hr(hr));
        return std::unexpected(e);
    }

    xml->put_async(VARIANT_FALSE);
    xml->put_validateOnParse(VARIANT_FALSE);
    xml->put_preserveWhiteSpace(VARIANT_TRUE);

    // This is required for XPATH
    hr = xml->setProperty(
        bstr_t(L"SelectionNamespaces"), variant_t("xmlns:xs='http://www.w3.org/2001/XMLSchema'"));
    if (FAILED(hr)) {
        LOG_CRITICAL("Failed setProperty on 'SelectionNamespaces'");
        XmlError e(make_error_code_from_hr(hr));
        return std::unexpected(e);
    }

    VARIANT_BOOL success;
    hr = xml->raw_loadXML(_bstr_t(text.toStdWString().c_str()), &success);
    if (FAILED(hr) || !success) {
        MSXML::IXMLDOMParseErrorPtr parseError;
        xml->get_parseError(&parseError);
        if (parseError) {
            parseError->get_errorCode(&hr);
            XmlError e{make_error_code_from_hr(hr)};

            e.location = XmlErrorLocation();
            _bstr_t message;
            parseError->get_reason(&message.GetBSTR());
            e.location->message = L"XSD Error: " + std::wstring(message);
            parseError->get_line(reinterpret_cast<long *>(&e.location->line));
            parseError->get_linepos(reinterpret_cast<long *>(&e.location->column));

            return std::unexpected(e);
        }

        XmlError e(std::error_code(hr, std::system_category()));
        return std::unexpected(e);
    }

    return xml;
}

QString CreateCaseInsensitiveAbsolutePath(const QString &xpath)
{
    QString path;

    for (const auto &item : xpath.split('/', Qt::SkipEmptyParts)) {
        if (item.startsWith('@')) {
            path = path + "/@*[translate(local-name(), 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', "
                   + "'abcdefghijklmnopqrstuvwxyz')='" + item.mid(1) + "']";
        } else {
            path = path + "/*[translate(local-name(), 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', "
                   + "'abcdefghijklmnopqrstuvwxyz')='" + item + "']";
        }
    }

    return path;
}

} // namespace

XmlDom::XmlDom() {}

XmlDom::~XmlDom() {}

std::expected<MSXML::IXMLDOMDocument3Ptr, XmlError> XmlDom::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_DEBUG(L"Could not open file (path: {}) [{}]", path, file.errorString());
        return std::unexpected(make_error_code(file.error()));
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();

    auto xml = ::CreateXmlDom(content);
    if (!xml) {
        LOG_DEBUG(L"Failed to parse (path: {}) [{}]", path, xml.error());
        return std::unexpected(xml.error());
    }

    xml_ = *xml;
    return xml_;
}

std::expected<MSXML::IXMLDOMDocument3Ptr, XmlError> XmlDom::loadFromBuffer(const QString &content)
{
    auto xml = ::CreateXmlDom(content);
    if (!xml) {
        LOG_DEBUG("Failed to parse XML buffer [{}]", xml.error());
        return std::unexpected(xml.error());
    }

    xml_ = *xml;
    return xml_;
}

std::expected<MSXML::IXMLDOMDocument3Ptr, std::string> XmlDom::get() const
{
    if (xml_ == nullptr) {
        return std::unexpected("XML with invalid COM pointer");
    }

    return xml_;
}

std::expected<MSXML::IXMLDOMDocument3Ptr, std::string> XmlDom::detach()
{
    if (xml_ == nullptr) {
        return std::unexpected("XML with invalid COM pointer");
    }

    auto ptr = xml_;
    xml_.Detach();
    return ptr;
}

std::expected<MSXML::IXMLDOMElementPtr, std::error_code> XmlDom::getRootElement()
{
    if (xml_ == nullptr) {
        // TODO: make a dummy element and do not check ?
        LOG_DEBUG("XML document is not initialized");
        return std::unexpected(std::make_error_code(std::errc::no_such_device));
    }

    MSXML::IXMLDOMElementPtr element;
    HRESULT hr = xml_->get_documentElement(&element);
    if (FAILED(hr)) {
        LOG_DEBUG("Failed to select root element");
        return std::unexpected(make_error_code_from_hr(hr));
    }

    return element;
}

std::expected<MSXML::IXMLDOMNodePtr, std::error_code> XmlDom::selectSingleNode(
    const QString &xpath) const
{
    if (!xml_) {
        LOG_DEBUG("XML document is not initialized");
        return std::unexpected(std::make_error_code(std::errc::no_such_device));
    }

    MSXML::IXMLDOMNodePtr node;
    HRESULT hr = xml_->raw_selectSingleNode(_bstr_t(xpath.toStdWString().c_str()), &node);
    if (FAILED(hr)) {
        LOG_DEBUG("Failed to select node (case sensitive: yes, path: {})", xpath);
        return std::unexpected(make_error_code_from_hr(hr));
    }

    if (!node) {
        auto path = ::CreateCaseInsensitiveAbsolutePath(xpath);
        hr = xml_->raw_selectSingleNode(_bstr_t(path.toStdWString().c_str()), &node);
        if (FAILED(hr)) {
            LOG_DEBUG("Failed to select node (case sensitive: no, path: {})", xpath);
            return std::unexpected(make_error_code_from_hr(hr));
        }
    }

    if (!node) {
        LOG_DEBUG("Node not found (path: {})", xpath);
        return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }

    return node;
}

std::expected<MSXML::IXMLDOMNodeListPtr, std::error_code> XmlDom::selectNodes(const QString &xpath)
{
    if (!xml_) {
        LOG_DEBUG("XML document is not initialized");
        return std::unexpected(std::make_error_code(std::errc::no_such_device));
    }

    MSXML::IXMLDOMNodeListPtr nodes;
    HRESULT hr = xml_->raw_selectNodes(_bstr_t(xpath.toStdWString().c_str()), &nodes);
    if (FAILED(hr)) {
        LOG_DEBUG("Failed to select nodes (path: {})", xpath);
        return std::unexpected(make_error_code_from_hr(hr));
    }

    return nodes;
}
