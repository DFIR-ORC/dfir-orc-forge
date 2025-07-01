//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include "editorinterface.h"
#include <QSplitter>

#include <optional>

#include <QString>
#include <QTabWidget>

class XsdStore;
class QTreeWidget;
class QTreeWidgetItem;

class ResourceEditor : public QSplitter, public EditorInterface
{
    Q_OBJECT

public:
    explicit ResourceEditor(XsdStore &xsdStore, QWidget *parent = nullptr);

    bool load(std::optional<QString> path) override;
    bool load(const QByteArray &data, EditorFormat format) override;
    bool save() override;
    bool saveAs() override;

    std::expected<QByteArray, std::error_code> content() const override;
    std::optional<QString> path() const override;

private:
    bool saveToFile(QString path);

public slots:
    bool openFileDialog();

private slots:
    void onResourceTreeItemClicked(QTreeWidgetItem *item, int column);

private:
    std::optional<QString> path_;
    XsdStore &xsdStore_;
    QTreeWidget *resourceTreeWidget_;
    QTabWidget *resourceEdition_;
};
