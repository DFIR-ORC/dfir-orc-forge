//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <QLabel>

struct XmlError;
struct IXMLDOMParseError;

enum class StatusType { Success = 0, Warning, Error };

class StatusLabel : public QLabel
{
    struct ColorScheme
    {
        QColor success;
        QColor warning;
        QColor error;
    };

public:
    explicit StatusLabel(QWidget *parent = nullptr);

    void setText(const QString &text, StatusType type);
    void setText(const XmlError &error, StatusType type);
    void setText(IXMLDOMParseError *error, StatusType type);
    static QString formatStatus(const QString &text);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    QPalette getPalette(StatusType type);
    void updateTheme();

private:
    StatusType m_currentType = StatusType::Success;
    ColorScheme m_theme;
    ColorScheme m_dark = {.success = Qt::darkGreen, .warning = Qt::yellow, .error = Qt::red};
    ColorScheme m_light = {.success = Qt::darkGreen, .warning = Qt::blue, .error = Qt::red};
};
