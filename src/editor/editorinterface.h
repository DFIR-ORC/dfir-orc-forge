//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <expected>

#include <QObject>

enum class EditorFormat { Unspecified, XmlConfiguration, YaraRule, DfirOrc, Unsupported };

class EditorSignals : public QObject
{
    Q_OBJECT
public:
signals:
    void contentsChanged();
    void fileSaved(QString path);
};

class EditorInterface
{
public:
    using Signals = EditorSignals;

    virtual ~EditorInterface();

    [[nodiscard]] virtual bool load(std::optional<QString> path = {}) = 0;
    [[nodiscard]] virtual bool load(
        const QByteArray &data, EditorFormat format = EditorFormat::Unspecified)
        = 0;
    [[nodiscard]] virtual bool save() = 0;
    [[nodiscard]] virtual bool saveAs() = 0;

    virtual std::expected<QByteArray, std::error_code> content() const = 0;
    virtual std::optional<QString> path() const = 0;

    EditorSignals &editorSignals();
    EditorSignals &editorSignals() const;

private:
    mutable EditorSignals editorSignals_;
};
