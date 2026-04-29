//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include "editorinterface.h"
#include "textedit/texteditwidget.h"

#include <optional>

#include <QString>

class XsdStore;
class XmlDom;
class XmlCompleter;

class XmlConfigEditor : public TextEditWidget, public EditorInterface
{
    Q_OBJECT

public:
    explicit XmlConfigEditor(XsdStore &xsdStore, QWidget *parent = nullptr);
    ~XmlConfigEditor();

    bool load(std::optional<QString> path) override;
    bool load(const QByteArray &data, EditorFormat format) override;
    bool save() override;
    bool saveAs() override;

    std::expected<QByteArray, std::error_code> content() const override;
    std::optional<QString> path() const override;

    std::optional<QString> getConfigIdentifier();

signals:
    void validationStatus(bool success, const QString &message);
    void configIdentifierChanged(const QString &identifier);

public slots:
    void validate();

private:
    bool saveToFile(QString path);

private:
    std::optional<QString> path_;
    XsdStore &xsdStore_;
    std::unique_ptr<XmlDom> dom_;
    QTimer validationTimer_;
    std::optional<QString> configIdentifier_;
    XmlCompleter *completer_ = nullptr;
};
