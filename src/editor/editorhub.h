//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include "editorinterface.h"
#include <QWidget>

#include <expected>

class QTabWidget;
class QToolBar;
class QPushButton;

class XsdStore;
class XmlConfigEditor;
class YaraEditor;

class EditionWidget : public QWidget, public EditorInterface
{
    Q_OBJECT
public:
    enum TabIndex { Text, Form };

    EditionWidget(XsdStore &xsdStore, QWidget *parent = nullptr);
    ~EditionWidget();

    bool load(std::optional<QString> path) override;
    bool load(const QByteArray &data, EditorFormat format) override;
    bool save() override;
    bool saveAs() override;

    std::expected<QByteArray, std::error_code> content() const;
    std::optional<QString> path() const override;

    bool isModified() const;

private:
    bool InitializeEditor(XmlConfigEditor *widget);
    bool InitializeEditor(YaraEditor *widget);
    bool InitializeUnsupportedEditor();

private:
    XsdStore &xsdStore_;
    QToolBar *toolbar_;
    QTabWidget *editors_;
    QPushButton *textButton_;
    bool isModified_;
};
