//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "xmlconfigeditor.h"

#include <comdef.h>
#include <msxml6.h>

#include <QFileDialog>
#include <QScrollbar>
#include <QTextDocument>
#include <QVBoxLayout>

#include "editor/xmlhighlighter.h"
#include "formatter/formatter.h"
#include "utils/log.h"
#include "xmldom.h"
#include "xsdstore.h"

namespace {

std::optional<QString> getConfigIdentifier(const QString &content)
{
    QRegularExpression elementRegex("<([a-zA-Z_:][a-zA-Z0-9_:.-]*)");

    QRegularExpressionMatch match = elementRegex.match(content);
    if (match.hasMatch()) {
        // The first capturing group contains the element name
        return match.captured(1);
    }

    return {};
}

} // namespace

XmlConfigEditor::XmlConfigEditor(XsdStore &xsdStore, QWidget *parent)
    : TextEditWidget(parent)
    , xsdStore_(xsdStore)
    , dom_(std::make_unique<XmlDom>())
{
    connect(
        editor_->document(),
        &QTextDocument::contentsChange,
        &editorSignals(),
        &EditorInterface::Signals::contentsChanged);

    connect(
        editor_->document(),
        &QTextDocument::contentsChange,
        this,
        [this](int position, int charsRemoved, int charsAdded) {
            if (charsRemoved > 0 || charsAdded > 0) {
                status_->clear();
                validationTimer_.singleShot(500, this, &XmlConfigEditor::validate);
            }
        });

    new XmlHighlighter(editor_->document());
}

XmlConfigEditor::~XmlConfigEditor() {}

bool XmlConfigEditor::load(std::optional<QString> path)
{
    if (!path) {
        path = QFileDialog::getOpenFileName(
            this, "Open File", "", "All Files (*.*);;XML configuration file (*.xml)");
    }

    QFile file(*path);
    if (!file.open(QIODevice::ReadWrite)) {
        LOG_CRITICAL("Failed to open file (path: {})", *path);
        return false;
    }

    editor_->setPlainText(QString::fromUtf8(file.readAll()));
    path_ = path;
    return true;
}

bool XmlConfigEditor::load(const QByteArray &data, EditorFormat format)
{
    if (format != EditorFormat::XmlConfiguration && format != EditorFormat::Unspecified) {
        return false;
    }

    editor_->setPlainText(QString::fromUtf8(data));
    return true;
}

bool XmlConfigEditor::save()
{
    auto path = path_;
    if (!path) {
        return saveAs();
    }

    if (!saveToFile(*path)) {
        return false;
    }

    return true;
}

bool XmlConfigEditor::saveAs()
{
    auto path = QFileDialog::getSaveFileName(
        this, "Save file", "", QString("File (*.xml);; All Files (*.*)"));
    if (path.isEmpty()) {
        return false;
    }

    if (!saveToFile(path)) {
        return false;
    }

    return true;
}

std::expected<QByteArray, std::error_code> XmlConfigEditor::content() const
{
    return editor_->toPlainText().toUtf8();
}

bool XmlConfigEditor::saveToFile(QString path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_CRITICAL("Failed to open file to save (path: {})", path);
        return false;
    }

    auto data = editor_->toPlainText().toUtf8();
    if (file.write(data) != data.size()) {
        return false;
    }

    path_ = path;
    emit editorSignals().fileSaved(path);
    return true;
}

std::optional<QString> XmlConfigEditor::path() const
{
    return path_;
}

std::optional<QString> XmlConfigEditor::getConfigIdentifier()
{
    return configIdentifier_;
}

void XmlConfigEditor::validate()
{
    QString content = editor_->toPlainText().toLower();

    auto id = ::getConfigIdentifier(content);
    if (!id) {
        status_->setText("Configuration identifier not found", StatusType::Error);
        return;
    }

    auto xsd = xsdStore_.get(*id);
    if (!xsd) {
        status_->setText(QString("Missing schema for '%1'").arg(*id), StatusType::Error);
        return;
    }

    if (*id != configIdentifier_) {
        configIdentifier_ = *id;
        emit configIdentifierChanged(*id);
    }

    auto document = dom_->loadFromBuffer(content);
    if (!document) {
        status_->setText(document.error(), StatusType::Error);
        return;
    }

    auto xml = document.value();
    auto schema = xsdStore_.getSchemaCache();
    if (!schema) {
        status_->setText("No schema available", StatusType::Error);
        return;
    }

    HRESULT hr = xml->putref_schemas(variant_t(schema));
    if (FAILED(hr)) {
        status_->setText("Cannot use schema", StatusType::Error);
        return;
    }

    MSXML::IXMLDOMParseErrorPtr parseError;
    hr = xml->raw_validate(&parseError);
    if (FAILED(hr)) {
        if (parseError) {
            status_->setText(parseError, StatusType::Error);
        } else {
            status_->setText("Unknown issue", StatusType::Error);
        }
    } else if (hr == S_FALSE) {
        if (parseError) {
            status_->setText(parseError, StatusType::Warning);
        } else {
            status_->setText("Unknown issue", StatusType::Warning);
        }
    } else {
        status_->setText("Validated with schema", StatusType::Success);
    }
}
