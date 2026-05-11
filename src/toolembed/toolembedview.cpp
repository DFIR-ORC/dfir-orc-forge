//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "toolembedview.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "embedfiles.h"
#include "processoutputview.h"
#include "toolembedsetupview.h"
#include "utils/log.h"

#include "toolembedview.moc"

ToolEmbedView::ToolEmbedView(QWidget *parent)
    : QWidget(parent)
    , stack_(new QStackedWidget(this))
    , setupView_(new ToolEmbedSetupView(this))
    , outputView_(new ProcessOutputView(this))
    , saveButton_(new QPushButton(tr("Save and open"), this))
{
    outputView_->setRunningMessage("Configuring DFIR-ORC...");
    outputView_->setSuccessMessage("Configuration succeeded.");
    outputView_->setFailureMessage("Configuration failed.");

    stack_->addWidget(setupView_);  // index 0 - setup panel
    stack_->addWidget(outputView_); // index 1 - live output
    stack_->setCurrentIndex(0);

    saveButton_->setDefault(true);
    saveButton_->setProperty("class", "primary");
    saveButton_->setEnabled(false);
    saveButton_->hide();

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(stack_);
    layout->addWidget(saveButton_);
    setLayout(layout);

    connect(
        setupView_,
        &ToolEmbedSetupView::readyToRun,
        this,
        [this](const QString &exe, const QString &embedConfig, const QString &configFolder) {
            run(exe, embedConfig, configFolder);
        });

    connect(outputView_, &ProcessOutputView::processSucceeded, this, [this](const QString &outPath) {
        saveButton_->show();
        saveButton_->setEnabled(true);
        // Re-connect each run so the captured outPath is always fresh
        disconnect(saveButton_, &QPushButton::clicked, nullptr, nullptr);
        connect(saveButton_, &QPushButton::clicked, this, [this, outPath]() {
            auto destPath = QFileDialog::getSaveFileName(
                this, tr("Save DFIR-ORC executable"), "DFIR-ORC.exe", tr("Executable (*.exe)"));
            if (destPath.isEmpty())
                return;

            QFile::remove(destPath);
            if (!QFile::copy(outPath, destPath)) {
                LOG_CRITICAL("Failed to copy output file to {}", destPath.toStdString());
                return;
            }

            saveButton_->setEnabled(false);
            emit fileReady(destPath);
        });
    });

    connect(outputView_, &ProcessOutputView::processFailed, this, [this]() {
        saveButton_->setEnabled(false);
        saveButton_->hide();
    });
}

void ToolEmbedView::promptAndRun()
{
    // Reset to the setup panel; the user interacts with it from here.
    stack_->setCurrentIndex(0);
    saveButton_->setEnabled(false);
    saveButton_->hide();
}

void ToolEmbedView::run(const QString &exe, const QString &embedConfig, const QString &configFolder)
{
    // Switch to the output panel so the user sees progress immediately.
    // All downloads have already been handled in ToolEmbedSetupView before
    // the Create button became enabled.
    stack_->setCurrentIndex(1);

    // Use a per-run counter so that successive Create runs within the same
    // session write to different temp files and do not collide.
    const auto outDir = QDir(
        QDir::tempPath()
        + QString("/dfir-orc-forge/%1/run%2")
              .arg(QCoreApplication::applicationPid())
              .arg(++runCounter_));
    if (!outDir.mkpath(".")) {
        LOG_CRITICAL("Failed to create output directory: {}", outDir.absolutePath().toStdString());
        stack_->setCurrentIndex(0);
        return;
    }
    const auto outPath = outDir.absoluteFilePath("DFIR-ORC.exe");

    outputView_->run(
        exe,
        {"toolembed",
         QString("/embed=%1").arg(embedConfig),
         QString("/out=%1").arg(outPath),
         "/force"},
        outPath);
}
