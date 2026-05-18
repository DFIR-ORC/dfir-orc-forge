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
#include <QTimer>

class YaraInstance;
class YaraCompiler;

class YaraEditor : public TextEditWidget, public EditorInterface
{
    Q_OBJECT

    static std::unique_ptr<YaraInstance> g_instance;

public:
    explicit YaraEditor(QWidget *parent = nullptr);
    ~YaraEditor();

    bool load(std::optional<QString> path) override;
    bool load(const QByteArray &data, EditorFormat format) override;
    bool save() override;
    bool saveAs() override;

    std::expected<QByteArray, std::error_code> content() const override;
    std::optional<QString> path() const override;

public slots:
    void validate();

private:
    void setupUI();
    void toggleLineComment();
    bool saveToFile(QString path);

private:
    std::optional<QString> path_;
    QTimer validationTimer_;
};
