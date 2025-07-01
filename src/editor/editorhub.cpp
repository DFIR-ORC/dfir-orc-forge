//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "editor/editorhub.h"

#include <cctype>
#include <format>
#include <regex>

#include <QButtonGroup>
#include <QFile>
#include <QFileDialog>
#include <QLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabBar>
#include <QTabWidget>
#include <QToolbar>

#include "editor/xmlconfigeditor.h"
#include "editor/yaraeditor.h"
#include "resource/resource.h"
#include "utils/log.h"

namespace {

bool isPrintableString(const QString &str)
{
    static const std::string_view allowedSpecial = "\n\t\r ";

    for (const QChar &ch : str) {
        if (!ch.isPrint() && allowedSpecial.find(ch.toLatin1()) == std::string_view::npos) {
            return false;
        }
    }

    return true;
}

bool isYaraRule(const QString &text)
{
    QRegularExpression pattern(R"(rule\s+[\w]+\s*\{[\s\S]*?\})");
    return pattern.match(text).hasMatch();
}

void SetTabWidget(QTabWidget *tabWidget, EditionWidget::TabIndex tabType, QWidget *widget)
{
    auto name = tabWidget->tabText(tabType);
    tabWidget->removeTab(tabType);
    tabWidget->insertTab(tabType, widget, name);
}

EditorFormat formatFromExtension(const QString &extension)
{
    static const QHash<QString, EditorFormat> extensionMap
        = {{"xml", EditorFormat::XmlConfiguration},
           {"yara", EditorFormat::YaraRule},
           {"txt", EditorFormat::YaraRule},
           {"", EditorFormat::YaraRule},
           {"exe", EditorFormat::DfirOrc}};

    return extensionMap.value(extension.toLower(), EditorFormat::Unspecified);
}

} // namespace

EditionWidget::EditionWidget(XsdStore &xsdStore, QWidget *parent)
    : QWidget(parent)
    , xsdStore_(xsdStore)
    , toolbar_(new QToolBar(this))
    , editors_(new QTabWidget(this))
    , textButton_(new QPushButton(this))
    , isModified_(false)
{
    textButton_->setText("Text");
    textButton_->setCheckable(true);
    connect(textButton_, &QPushButton::clicked, [this] {
        editors_->setCurrentIndex(TabIndex::Text);
    });

    auto buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(textButton_);
    buttonGroup->setExclusive(true);

    toolbar_->addWidget(textButton_);
    toolbar_->setVisible(false);
    toolbar_->setContentsMargins(0, 0, 0, 0);

    editors_->tabBar()->setVisible(false);
    editors_->addTab(nullptr, "Text");
    editors_->addTab(nullptr, "Form");
    editors_->setContentsMargins(0, 0, 0, 0);
    editors_->setDocumentMode(true);

    setLayout(new QVBoxLayout(this));

    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->setSpacing(0);

    layout()->addWidget(toolbar_);
    layout()->addWidget(editors_);
}

EditionWidget::~EditionWidget() {}

bool EditionWidget::InitializeEditor(XmlConfigEditor *xmlEditor)
{
    connect(&xmlEditor->editorSignals(), &EditorInterface::Signals::contentsChanged, [this]() {
        isModified_ = true;
        emit editorSignals().contentsChanged();
    });

    connect(
        &xmlEditor->editorSignals(),
        &EditorInterface::Signals::fileSaved,
        this,
        [this](const QString &path) {
            isModified_ = false;
            emit editorSignals().fileSaved(path);
        });

    xmlEditor->validate();
    return true;
}

bool EditionWidget::InitializeEditor(YaraEditor *yaraEditor)
{
    connect(&yaraEditor->editorSignals(), &EditorInterface::Signals::contentsChanged, [this]() {
        isModified_ = true;
        emit editorSignals().contentsChanged();
    });

    connect(
        &yaraEditor->editorSignals(),
        &EditorInterface::Signals::fileSaved,
        this,
        [this](const QString &path) {
            isModified_ = false;
            emit editorSignals().fileSaved(path);
        });

    return true;
}

bool EditionWidget::InitializeUnsupportedEditor()
{
    auto *editor = new QTextEdit(this);
    editor->setEnabled(false);
    editor->setPlainText("<unsupported format>");
    ::SetTabWidget(editors_, TabIndex::Text, editor);
    return true;
}

bool EditionWidget::load(std::optional<QString> path)
{
    if (!path) {
        path = QFileDialog::getOpenFileName(
            this,
            "Open File",
            "",
            "All Files (*.*);;YARA Rule Files (*.yara);;Configuration file (*.xml)");
    }

    auto format = formatFromExtension(QFileInfo(*path).suffix());
    switch (format) {
    case EditorFormat::XmlConfiguration: {
        auto *xmlEditor = new XmlConfigEditor(xsdStore_);
        ::SetTabWidget(editors_, TabIndex::Text, xmlEditor);
        bool loaded = xmlEditor->load(*path);
        if (!loaded) {
            return false;
        }

        return InitializeEditor(xmlEditor);
    }
    case EditorFormat::YaraRule: {
        auto yaraEditor = new YaraEditor();
        ::SetTabWidget(editors_, TabIndex::Text, yaraEditor);
        bool loaded = yaraEditor->load(*path);
        if (!loaded) {
            return false;
        }

        return InitializeEditor(yaraEditor);
    }
    case EditorFormat::Unsupported:
    case EditorFormat::Unspecified: {
        // TODO: detect if text/xml/yara ?
        return InitializeUnsupportedEditor();
    }
    }

    return false;
}

bool EditionWidget::load(const QByteArray &data, EditorFormat format)
{
    if (format == EditorFormat::Unspecified) {
        auto text = QString::fromUtf8(data);
        if (isPrintableString(text)) {
            if (isYaraRule(text)) {
                format = EditorFormat::YaraRule;
            } else {
                format = EditorFormat::XmlConfiguration;
            }
        }
    }

    switch (format) {
    case EditorFormat::XmlConfiguration: {
        auto *xmlEditor = new XmlConfigEditor(xsdStore_);
        ::SetTabWidget(editors_, TabIndex::Text, xmlEditor);
        bool loaded = xmlEditor->load(data, format);
        if (!loaded) {
            return false;
        }
        return InitializeEditor(xmlEditor);
    }
    case EditorFormat::YaraRule: {
        auto yaraEditor = new YaraEditor();
        ::SetTabWidget(editors_, TabIndex::Text, yaraEditor);
        bool loaded = yaraEditor->load(data, format);
        if (!loaded) {
            return false;
        }

        return InitializeEditor(yaraEditor);
    }
    default:
        return InitializeUnsupportedEditor();
    }
}

bool EditionWidget::save()
{
    // BEWARE: loop over all text editors and stop on the first content fetching
    for (size_t i = 0; i < editors_->count(); ++i) {
        auto *widget = editors_->widget(i);
        if (!widget) {
            continue;
        }

        auto *editor = dynamic_cast<EditorInterface *>(widget);
        if (editor) {
            return editor->save();
        }
    }

    return false;
}

bool EditionWidget::saveAs()
{
    // BEWARE: loop over all text editors and stop on the first content fetching
    for (size_t i = 0; i < editors_->count(); ++i) {
        auto *widget = editors_->widget(i);
        if (!widget) {
            continue;
        }

        auto *editor = dynamic_cast<EditorInterface *>(widget);
        if (editor) {
            return editor->saveAs();
        }
    }

    return false;
}

std::expected<QByteArray, std::error_code> EditionWidget::content() const
{
    // BEWARE: loop over all text editors and stop on the first content fetching
    for (size_t i = 0; i < editors_->count(); ++i) {
        auto *widget = editors_->widget(i);
        if (!widget) {
            continue;
        }

        auto *editor = dynamic_cast<EditorInterface *>(widget);
        if (editor) {
            return editor->content();
        }
    }

    LOG_DEBUG("Cannot find 'EditionInterface'");
    return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
}

std::optional<QString> EditionWidget::path() const
{
    // Loop over all text editors and stop on the first content fetching
    for (size_t i = 0; i < editors_->count(); ++i) {
        auto *widget = editors_->widget(i);
        if (!widget) {
            continue;
        }

        auto *editor = dynamic_cast<EditorInterface *>(widget);
        if (editor && editor->path()) {
            return editor->path();
        }
    }

    return {};
}

bool EditionWidget::isModified() const
{
    return isModified_;
}
