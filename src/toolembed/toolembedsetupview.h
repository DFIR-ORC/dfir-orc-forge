//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#pragma once

#include <expected>
#include <QList>
#include <QString>
#include <QWidget>

#include "embedfiles.h"

class QLabel;
class QPushButton;
class QStackedWidget;
class QTreeWidget;

// Wizard-style setup panel shown before the toolembed process runs.
//
// Step 1 - Executable  : choose the DFIR-ORC base executable (embedded or browse).
// Step 2 - Configuration: choose the configuration folder.
// Step 3 - Files        : review / download third-party files, then create.
//
// Emits readyToRun(...) when the user confirms on the last step.
// All three pages accept drag-and-drop:
//   Page 0 -- drop a .exe file to set the DFIR-ORC executable.
//   Page 1 -- drop a directory to set the configuration folder.
//   Page 2 -- drop a directory to resolve missing binaries from it.
class ToolEmbedSetupView : public QWidget
{
    Q_OBJECT

public:
    explicit ToolEmbedSetupView(QWidget *parent = nullptr);

    // Resolved paths (empty when not yet selected / not embedded).
    QString exe() const { return exe_; }
    QString embedConfig() const { return embedConfig_; }
    QString configFolder() const { return configFolder_; }

signals:
    void readyToRun(QString exe, QString embedConfig, QString configFolder);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onChangeExe();
    void onChangeConfig();
    void onImportConfig();
    void onImportBinaries();
    void onTreeContextMenu(const QPoint &pos);
    void onBrowseLocalDir();

private:
    // Wizard navigation
    void goToStep(int step);
    void updateNavButtons();

    // Data probing / row refresh
    EulaPromptFn makeEulaPrompt();
    void probeEmbeddedResources();
    void refreshExePage();
    void refreshConfigPage();
    void refreshDownloadsPage();
    void refreshCreateButton();
    void refreshDownloadButton();

    // Copies a file from localDir_ whose basename matches f.dest into f.dest.
    // Returns true on success, false if the file is not found or the copy fails.
    bool installFromLocalDir(const EmbedFile &f);

    // Runs "<srcExe> toolembed /dump /out=<destDir>" synchronously.
    // Returns the dump output directory on success, or an error message.
    std::expected<QString, QString> runToolembedDump(const QString &srcExe, const QString &destDir);

    // Applies a dropped local directory as the binary source for page 2.
    void applyLocalDir(const QString &dir);

    // ---- state ----
    QString tempBaseDir_;
    int currentStep_ = 0; // 0=exe  1=config  2=files

    QString exe_;
    QString configFolder_;
    QString embedConfig_;
    bool configIsEmbedded_ = false;

    QString localDir_; // last directory chosen for local binary lookup

    QList<EmbedFile> allFiles_;

    // ---- wizard chrome ----
    QStackedWidget *pages_;

    // Step 0 - Executable
    QLabel *exeIcon_;
    QLabel *exeLabel_;
    QLabel *exeProductLabel_;
    QLabel *exeDesc_;

    // Step 1 - Configuration
    QLabel *configIcon_;
    QLabel *configLabel_;
    QLabel *configDesc_;
    QPushButton *importConfigButton_;

    // Step 2 - Third-party files
    QTreeWidget *downloadsTree_;
    QLabel *filesStatusLabel_;
    QLabel *filesDesc_;

    // Navigation
    QPushButton *backButton_;
    QPushButton *nextButton_;
    QPushButton *createButton_;
    QPushButton *downloadButton_;
    QPushButton *locateButton_;
    QPushButton *importBinariesButton_;
};
