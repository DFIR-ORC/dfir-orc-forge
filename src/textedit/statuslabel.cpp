//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "statuslabel.h"

#include <QApplication>

#include "utils/xmlerror.h"

StatusLabel::StatusLabel(QWidget *parent)
    : QLabel(parent)
{
    QApplication::instance()->installEventFilter(this);
    updateTheme();
    setMargin(5);
}

void StatusLabel::setText(const QString &text, StatusType type)
{
    setPalette(getPalette(type));
    QLabel::setText(formatStatus(text));
}

void StatusLabel::setText(const XmlError &error, StatusType type)
{
    setText(error.toString(), type);
}

void StatusLabel::setText(IXMLDOMParseError *error, StatusType type)
{
    setText(XmlError(*error), type);
}

QString StatusLabel::formatStatus(const QString &text)
{
    auto message = text;
    message.replace("\r\n", " ");
    message = message.simplified();
    if (!message.isEmpty() && message[0].isLower()) {
        message[0] = message[0].toUpper();
    }

    return message;
}

bool StatusLabel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange) {
        updateTheme();
        return false;
    }

    return QObject::eventFilter(watched, event);
}

QPalette StatusLabel::getPalette(StatusType type)
{
    QColor color;
    switch (type) {
    case StatusType::Success:
        color = m_theme.success;
        break;
    case StatusType::Warning:
        color = m_theme.warning;
        break;
    case StatusType::Error:
        color = m_theme.error;
        break;
    }

    QPalette palette = this->palette();
    palette.setColor(foregroundRole(), color);
    return palette;
}

void StatusLabel::updateTheme()
{
    bool isDarkTheme = QApplication::palette().color(QPalette::Window).lightness() < 128;
    m_theme = isDarkTheme ? m_dark : m_light;
}
