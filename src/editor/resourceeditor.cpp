//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "editor/resourceeditor.h"

#include <QFileDialog>
#include <QTreeWidget>

#include "editor/editorhub.h"
#include "formatter/formatter.h"
#include "resource/resource.h"
#include "resource/resourcedirectory.h"
#include "utils/log.h"

namespace {

class EditionContext
{
public:
    EditionContext(const QString &path, const ResourceInfo &resource)
        : path_(path)
        , resource_(resource)
        , tabIndex_()
    {}

    std::optional<int> TabIndex() const { return tabIndex_; }
    void SetTabIndex(int index) { tabIndex_ = index; }

    const QString &Path() const { return path_; }
    const ResourceInfo &Resource() const { return resource_; }

private:
    QString path_;
    ResourceInfo resource_;
    std::optional<int> tabIndex_;
};

} // namespace

ResourceEditor::ResourceEditor(XsdStore &xsdStore, QWidget *parent)
    : QSplitter(parent)
    , xsdStore_(xsdStore)
{
    resourceTreeWidget_ = new QTreeWidget(this);
    resourceTreeWidget_->setHeaderHidden(true);
    resourceTreeWidget_->setColumnCount(1);
    resourceTreeWidget_->setColumnWidth(0, 200);
    addWidget(resourceTreeWidget_);

    resourceEdition_ = new QTabWidget(this);
    resourceEdition_->setDocumentMode(
        true); // BEWARE: create a glitch if 'true' when QFrame style != NoFrame
    resourceEdition_->tabBar()->setVisible(false);
    addWidget(resourceEdition_);

    setStretchFactor(0, 0);
    setStretchFactor(1, 1);
    setHandleWidth(2);
    setContentsMargins(0, 0, 0, 0);

    connect(
        resourceTreeWidget_,
        &QTreeWidget::itemClicked,
        this,
        &ResourceEditor::onResourceTreeItemClicked);
}

bool ResourceEditor::openFileDialog()
{
    auto path = QFileDialog::getOpenFileName(
        this, "Open File", "", "All Files (*.*);;Executable file (*.exe)");
    return load(path);
}

bool ResourceEditor::load(std::optional<QString> path)
{
    if (!path) {
        path = QFileDialog::getOpenFileName(
            this, "Open File", "", "All Files (*.*);;Executable file (*.xml)");
    }

    auto resources = std::make_unique<ResourceDirectory>();
    if (!resources->loadFromFile(path->toStdWString())) {
        LOG_CRITICAL("Failed to load resources");
        return false;
    }

    for (auto &typeDir : resources->TypeDirectories()) {
        auto *typeDirItem = new QTreeWidgetItem(resourceTreeWidget_);
        auto text = QString(ResourceDirectory::GetFriendlyType(typeDir.name));
        typeDirItem->setText(0, text);
        typeDirItem->setData(0, Qt::UserRole, QVariant::fromValue(&typeDir));
        if (text == QString("BINARY")) {
            typeDirItem->setExpanded(true);
        }

        for (auto &nameDir : resources->NameDirectories(typeDir)) {
            auto *nameDirItem = new QTreeWidgetItem(typeDirItem);
            nameDirItem->setText(0, QString(nameDir.name));

            if (QString::fromStdWString(nameDir.name).endsWith("SQLSCHEMA")) {
                nameDirItem->setHidden(true);
            }

            if (resources->LangDirectories(nameDir).size() == 1) {
                auto resource = ResourceInfo(resources->LangDirectories(nameDir)[0]);
                auto context = std::make_shared<::EditionContext>(*path, resource);
                nameDirItem->setData(0, Qt::UserRole, QVariant::fromValue(std::move(context)));
                continue;
            }

            for (auto &langDir : resources->LangDirectories(nameDir)) {
                auto *langDirItem = new QTreeWidgetItem(nameDirItem);
                langDirItem->setText(0, QString::number(langDir.langId));

                auto resource = ResourceInfo(langDir);
                auto context = std::make_shared<::EditionContext>(*path, resource);
                langDirItem->setData(0, Qt::UserRole, QVariant::fromValue(std::move(context)));
            }
        }
    }

    path_ = path;
    return true;
}

bool ResourceEditor::load(const QByteArray &data, EditorFormat format)
{
    LOG_CRITICAL("Not implemented"); // Could do it using %TEMP% ?
    return false;
}

bool ResourceEditor::save()
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

bool ResourceEditor::saveAs()
{
    auto path = QFileDialog::getSaveFileName(
        this, "Save file", "", QString("File (*.exe);; All Files (*.*)"));
    if (path.isEmpty()) {
        return false;
    }

    if (!saveToFile(path)) {
        return false;
    }

    return true;
}

bool ResourceEditor::saveToFile(QString path)
{
    if (path_ && QFileInfo(*path_).canonicalFilePath() != QFileInfo(path).canonicalFilePath()) {
        if (QFile::exists(path)) {
            QFile::remove(path);
        }

        if (!QFile::copy(*path_, path)) {
            LOG_CRITICAL(
                "Failed to copy source file while saving (source: {}, destination: {})",
                *path_,
                path);
            return false;
        }
    }

    std::vector<Resource> resourceItems;
    QTreeWidgetItemIterator it(resourceTreeWidget_);
    for (; *it; ++it) {
        QTreeWidgetItem *item = *it;
        auto context = item->data(0, Qt::UserRole).value<std::shared_ptr<::EditionContext>>();
        if (!context) {
            continue;
        }

        if (!context->TabIndex()) {
            continue;
        }

        auto edition = qobject_cast<EditionWidget *>(resourceEdition_->widget(*context->TabIndex()));
        if (!edition) {
            continue;
        }

        if (!edition->isModified()) {
            continue;
        }

        std::vector<uint8_t> data;
        auto editionContent = edition->content();
        if (!editionContent) {
            LOG_CRITICAL("Failed to save file: cannot retrieve editor's content");
            return false;
        }

        std::copy(std::cbegin(*editionContent), std::cend(*editionContent), std::back_inserter(data));
        resourceItems.emplace_back(context->Resource(), std::move(data));
    }

    auto status = Resource::UpdateResources(path.toStdWString(), resourceItems);
    if (!status) {
        LOG_CRITICAL("Failed to save file: cannot update resources");
        return false;
    }

    path_ = path;
    emit editorSignals().fileSaved(path);
    return true;
}

std::expected<QByteArray, std::error_code> ResourceEditor::content() const
{
    // This is not required
    return std::unexpected(std::make_error_code(std::errc::not_supported));
}

std::optional<QString> ResourceEditor::path() const
{
    return path_;
}

void ResourceEditor::onResourceTreeItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item) {
        return;
    }

    auto context = item->data(0, Qt::UserRole).value<std::shared_ptr<::EditionContext>>();
    if (!context) {
        return;
    }

    if (context->TabIndex()) {
        resourceEdition_->setCurrentIndex(*context->TabIndex());
        return;
    }

    auto resourceData
        = Resource::GetResourceData(context->Path().toStdWString(), context->Resource());
    if (!resourceData) {
        return;
    }

    auto edition = new EditionWidget(xsdStore_, this);
    if (!edition->load(
            QByteArray::fromRawData(
                reinterpret_cast<const char *>(resourceData->data()), resourceData->size()),
            EditorFormat::Unspecified)) {
        return;
    }

    auto tabIndex
        = resourceEdition_
              ->addTab(edition, QString::fromStdWString(context->Resource().FriendlyName()));
    context->SetTabIndex(tabIndex);
    resourceEdition_->setCurrentIndex(tabIndex);

    connect(
        &edition->editorSignals(),
        &EditorInterface::Signals::contentsChanged,
        &editorSignals(),
        &EditorInterface::Signals::contentsChanged);
}
