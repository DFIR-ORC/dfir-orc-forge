//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "utils/xmlerror.h"

#include <msxml6.h>

#include "utils/guard.h"

XmlError::XmlError(const std::error_code &e)
    : error(e)
{}

XmlError::XmlError(int value, const std::error_category &category)
    : error(value, category)
{}

XmlError::XmlError(std::errc &errc)
    : error(std::make_error_code(errc))
{}

XmlError::XmlError(IXMLDOMParseError &parseError)
{
    location = XmlErrorLocation();

    error = std::make_error_code(std::errc::bad_message);
    parseError.get_line(reinterpret_cast<long *>(&location->line));
    parseError.get_linepos(reinterpret_cast<long *>(&location->column));

    Guard::Bstr reason;
    parseError.get_reason(reason.addressOf());
    location->message = reason ? reason.get() : L"";
}

XmlError::XmlError(const MSXML::IXMLDOMParseErrorPtr &parseError)
{
    location = XmlErrorLocation();

    error = std::make_error_code(std::errc::bad_message);
    parseError->get_line(reinterpret_cast<long *>(&location->line));
    parseError->get_linepos(reinterpret_cast<long *>(&location->column));

    Guard::Bstr reason;
    parseError->get_reason(reason.addressOf());
    location->message = reason ? reason.get() : L"";
}

QString XmlError::toString() const
{
    if (!error && !location) {
        return {};
    }

    if (!location) {
        return QString::fromStdString(error.message());
    }

    if (location->line == 0 && location->column == 0) {
        return QString::fromStdWString(location->message);
    }

    QString message = QString::fromStdWString(location->message);
    message.remove("\r\n");
    if (message.endsWith('.')) {
        message = message.removeLast();
    }

    return QString("%1 [L%2:C%3]").arg(message).arg(location->line).arg(location->column);
}
