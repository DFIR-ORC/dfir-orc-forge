//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "toolembedsetupview.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <windows.h>

#include "embedconfiguration.h"
#include "embedfiles.h"
#include "resource/resource.h"
#include "utils/log.h"
#include "utils/peinfo.h"

#include "toolembedsetupview.moc"

namespace {

// Returns a coloured bullet: green (ok) or red (error).
QString okIcon()
{
    return QStringLiteral("<span style='color:#3c3'>&#9679;</span>");
}
QString errIcon()
{
    return QStringLiteral("<span style='color:#c33'>&#9679;</span>");
}

} // namespace

// ---------------------------------------------------------------------------
// Constructor -- builds wizard chrome + three pages
// ---------------------------------------------------------------------------
ToolEmbedSetupView::ToolEmbedSetupView(QWidget *parent)
    : QWidget(parent)
    , tempBaseDir_(
          QDir::tempPath() + QString("/dfir-orc-forge/%1").arg(QCoreApplication::applicationPid()))
    , pages_(new QStackedWidget(this))
    , exeIcon_(new QLabel(this))
    , exeLabel_(new QLabel(this))
    , exeProductLabel_(new QLabel(this))
    , configIcon_(new QLabel(this))
    , configLabel_(new QLabel(this))
    , downloadsTree_(new QTreeWidget(this))
    , filesStatusLabel_(new QLabel(this))
    , backButton_(new QPushButton(tr("< Back"), this))
    , nextButton_(new QPushButton(tr("Next >"), this))
    , createButton_(new QPushButton(tr("Create"), this))
    , downloadButton_(new QPushButton(tr("Download missing files"), this))
    , locateButton_(new QPushButton(tr("Browse directory"), this))
    , importBinariesButton_(new QPushButton(tr("Extract from DFIR-ORC"), this))
    , importConfigButton_(new QPushButton(tr("Extract from DFIR-ORC"), this))
    , exeDesc_(new QLabel(this))
    , configDesc_(new QLabel(this))
    , filesDesc_(new QLabel(this))
{
    setAcceptDrops(true);

    // ------------------------------------------------------------------ //
    // Page 0 -- Executable
    // ------------------------------------------------------------------ //
    auto *exePage = new QWidget(this);
    auto *exePageLayout = new QVBoxLayout(exePage);
    exePageLayout->setContentsMargins(0, 0, 0, 0);
    exePageLayout->setSpacing(0);

    auto *exePanel = new QWidget(exePage);
    auto *exeLayout = new QVBoxLayout(exePanel);
    exeLayout->setContentsMargins(12, 16, 12, 16);
    exeLayout->setSpacing(0);

    auto *exeTitle = new QLabel(tr("DFIR-ORC Executable"), exePanel);
    {
        QFont f = exeTitle->font();
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() * 1.1);
        exeTitle->setFont(f);
    }

    exeDesc_->setParent(exePanel);
    exeDesc_->setText(tr("Select the base DFIR-ORC executable that will be configured and "
                         "repackaged.  Use \"Browse executable\" to pick one from disk.  "
                         "When an executable is embedded in Forge it is pre-selected "
                         "automatically.  You can also drag and drop a .exe file here."));
    exeDesc_->setWordWrap(true);
    exeDesc_->setObjectName("pageDescription");
    auto *exeDesc = exeDesc_;

    // Row 1: [dot] [path label] [Browse]
    auto *exeRow = new QHBoxLayout;
    exeRow->setSpacing(6);
    exeIcon_->setFixedWidth(20);
    exeIcon_->setTextFormat(Qt::RichText);
    exeIcon_->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    exeLabel_->setTextFormat(Qt::RichText);
    exeLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *changeExeBtn = new QPushButton(tr("Browse executable"), exePanel);
    changeExeBtn->setFixedWidth(160);
    changeExeBtn->setToolTip(tr("Select a DFIR-ORC executable from disk."));
    connect(changeExeBtn, &QPushButton::clicked, this, &ToolEmbedSetupView::onChangeExe);
    exeRow->addWidget(exeIcon_);
    exeRow->addWidget(exeLabel_, 1);
    exeRow->addWidget(changeExeBtn);

    // Row 2: indented product name
    {
        QPalette p = exeProductLabel_->palette();
        p.setColor(QPalette::WindowText, p.color(QPalette::Disabled, QPalette::WindowText));
        exeProductLabel_->setPalette(p);
        QFont f = exeProductLabel_->font();
        f.setPointSizeF(f.pointSizeF() * 0.88);
        exeProductLabel_->setFont(f);
    }
    exeProductLabel_->setIndent(26); // aligns under the path label (past dot + spacing)
    exeProductLabel_->hide();

    auto *exeSep = new QFrame(exePanel);
    exeSep->setFrameShape(QFrame::HLine);
    exeSep->setFrameShadow(QFrame::Sunken);

    exeLayout->addStretch(1);
    exeLayout->addWidget(exeTitle);
    exeLayout->addSpacing(6);
    exeLayout->addWidget(exeDesc);
    exeLayout->addSpacing(12);
    exeLayout->addWidget(exeSep);
    exeLayout->addSpacing(10);
    exeLayout->addLayout(exeRow);
    exeLayout->addWidget(exeProductLabel_);
    exeLayout->addStretch(1);

    auto *exeHBox = new QHBoxLayout;
    exeHBox->setSpacing(0);
    exeHBox->addStretch(1);
    exeHBox->addWidget(exePanel, 8);
    exeHBox->addStretch(1);
    exePageLayout->addStretch(1);
    exePageLayout->addLayout(exeHBox, 8);
    exePageLayout->addStretch(1);

    // ------------------------------------------------------------------ //
    // Page 1 -- Configuration
    // ------------------------------------------------------------------ //
    auto *configPage = new QWidget(this);
    auto *configPageLayout = new QVBoxLayout(configPage);
    configPageLayout->setContentsMargins(0, 0, 0, 0);
    configPageLayout->setSpacing(0);

    auto *configPanel = new QWidget(configPage);
    auto *configLayout = new QVBoxLayout(configPanel);
    configLayout->setContentsMargins(12, 16, 12, 16);
    configLayout->setSpacing(0);

    auto *configTitle = new QLabel(tr("Configuration Folder"), configPanel);
    {
        QFont f = configTitle->font();
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() * 1.1);
        configTitle->setFont(f);
    }

    configDesc_->setParent(configPanel);
    configDesc_->setText(
        tr("Select the folder that contains the toolembed configuration "
           "(embed.xml or any XML file with a <toolembed> root element).  "
           "Use \"Browse folder...\" to pick a configuration directory directly, "
           "or \"Import from exe...\" to extract the configuration embedded "
           "inside an existing DFIR-ORC executable.  "
           "When a configuration is embedded in Forge it is pre-selected "
           "automatically.  You can also drag and drop a configuration folder here."));
    configDesc_->setWordWrap(true);
    configDesc_->setObjectName("pageDescription");
    auto *configDesc = configDesc_;

    auto *configRow = new QHBoxLayout;
    configRow->setSpacing(6);
    configIcon_->setFixedWidth(20);
    configIcon_->setTextFormat(Qt::RichText);
    configIcon_->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    configLabel_->setTextFormat(Qt::RichText);
    configLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *changeConfigBtn = new QPushButton(tr("Browse directory"), configPanel);
    changeConfigBtn->setFixedWidth(160);
    changeConfigBtn->setToolTip(
        tr("Select a local folder that already contains a toolembed configuration."));
    connect(changeConfigBtn, &QPushButton::clicked, this, &ToolEmbedSetupView::onChangeConfig);
    importConfigButton_->setFixedWidth(160);
    importConfigButton_->setToolTip(tr(
        "Pick an existing configured DFIR-ORC executable to extract its embedded configuration."));
    connect(importConfigButton_, &QPushButton::clicked, this, &ToolEmbedSetupView::onImportConfig);
    configRow->addWidget(configIcon_);
    configRow->addWidget(configLabel_, 1);
    configRow->addWidget(changeConfigBtn);
    configRow->addWidget(importConfigButton_);

    auto *configSep = new QFrame(configPanel);
    configSep->setFrameShape(QFrame::HLine);
    configSep->setFrameShadow(QFrame::Sunken);

    configLayout->addStretch(1);
    configLayout->addWidget(configTitle);
    configLayout->addSpacing(6);
    configLayout->addWidget(configDesc);
    configLayout->addSpacing(12);
    configLayout->addWidget(configSep);
    configLayout->addSpacing(10);
    configLayout->addLayout(configRow);
    configLayout->addStretch(1);

    auto *configHBox = new QHBoxLayout;
    configHBox->setSpacing(0);
    configHBox->addStretch(1);
    configHBox->addWidget(configPanel, 8);
    configHBox->addStretch(1);
    configPageLayout->addStretch(1);
    configPageLayout->addLayout(configHBox, 8);
    configPageLayout->addStretch(1);

    // ------------------------------------------------------------------ //
    // Page 2 -- Third-party files
    // ------------------------------------------------------------------ //
    auto *filesPage = new QWidget(this);
    auto *filesPageLayout = new QVBoxLayout(filesPage);
    filesPageLayout->setContentsMargins(0, 0, 0, 0);
    filesPageLayout->setSpacing(0);

    auto *filesPanel = new QWidget(filesPage);
    auto *filesLayout = new QVBoxLayout(filesPanel);
    filesLayout->setContentsMargins(12, 16, 12, 12);
    filesLayout->setSpacing(0);

    auto *filesTitle = new QLabel(tr("Third-party software to embed"), filesPanel);
    {
        QFont f = filesTitle->font();
        f.setBold(true);
        f.setPointSizeF(f.pointSizeF() * 1.1);
        filesTitle->setFont(f);
    }

    filesDesc_->setParent(filesPanel);
    filesDesc_->setText(tr("The table below lists the third-party executables referenced by the "
                           "configuration.  Files marked as Missing must be downloaded (or "
                           "placed manually) before the creation can proceed.  Right-click a "
                           "row to download or copy the source URL.  You can also drag and drop "
                           "a directory containing the binaries here."));
    filesDesc_->setWordWrap(true);
    filesDesc_->setObjectName("pageDescription");
    auto *filesDesc = filesDesc_;

    downloadsTree_->setColumnCount(3);
    downloadsTree_->setHeaderLabels({tr("File"), tr("Source URL"), tr("Status")});
    downloadsTree_->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    downloadsTree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    downloadsTree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    downloadsTree_->header()->setMinimumSectionSize(60);
    downloadsTree_->header()->setDefaultSectionSize(180);
    downloadsTree_->setRootIsDecorated(false);
    downloadsTree_->setAlternatingRowColors(true);
    downloadsTree_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    downloadsTree_->setTextElideMode(Qt::ElideMiddle);
    downloadsTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        downloadsTree_,
        &QTreeWidget::customContextMenuRequested,
        this,
        &ToolEmbedSetupView::onTreeContextMenu);

    filesStatusLabel_->setWordWrap(true);
    filesStatusLabel_->setObjectName("filesStatus");

    filesLayout->addWidget(filesTitle);
    filesLayout->addSpacing(6);
    filesLayout->addWidget(filesDesc);
    filesLayout->addSpacing(12);
    filesLayout->addWidget(downloadsTree_, 1);
    filesLayout->addSpacing(4);
    filesLayout->addWidget(filesStatusLabel_);

    auto *downloadRow = new QHBoxLayout;
    downloadRow->setSpacing(8);
    locateButton_->setFixedWidth(150);
    locateButton_->setToolTip(tr("Select a local folder containing the binary files."));
    importBinariesButton_->setFixedWidth(150);
    importBinariesButton_->setToolTip(
        tr("Pick an existing configured DFIR-ORC executable to extract the embedded tools from."));
    downloadButton_->setFixedWidth(150);
    downloadButton_->setToolTip(tr("Fetch the missing files from their configured URL."));
    downloadRow->addStretch(1);
    downloadRow->addWidget(locateButton_);
    downloadRow->addWidget(importBinariesButton_);
    downloadRow->addWidget(downloadButton_);
    filesLayout->addSpacing(4);
    filesLayout->addLayout(downloadRow);

    connect(locateButton_, &QPushButton::clicked, this, &ToolEmbedSetupView::onBrowseLocalDir);
    connect(importBinariesButton_, &QPushButton::clicked, this, &ToolEmbedSetupView::onImportBinaries);

    auto *filesHBox = new QHBoxLayout;
    filesHBox->setSpacing(0);
    filesHBox->addStretch(1);
    filesHBox->addWidget(filesPanel, 8);
    filesHBox->addStretch(1);

    filesPageLayout->addStretch(1);
    filesPageLayout->addLayout(filesHBox, 8);
    filesPageLayout->addStretch(1);

    connect(downloadButton_, &QPushButton::clicked, this, [this]() {
        if (auto result = ResolveEmbedDownloads(embedConfig_, configFolder_, makeEulaPrompt());
            !result) {
            QMessageBox::critical(this, tr("Download failed"), result.error());
        }
        refreshDownloadsPage();
        refreshDownloadButton();
        updateNavButtons();
    });

    // ------------------------------------------------------------------ //
    // Assemble pages
    // ------------------------------------------------------------------ //
    pages_->addWidget(exePage);    // 0
    pages_->addWidget(configPage); // 1
    pages_->addWidget(filesPage);  // 2
    pages_->setCurrentIndex(0);

    // ------------------------------------------------------------------ //
    // Navigation button row (installer style: separator + flush-right buttons)
    // ------------------------------------------------------------------ //
    backButton_->setEnabled(false);
    backButton_->setFixedWidth(80);
    nextButton_->setDefault(true);
    nextButton_->setFixedWidth(80);
    createButton_->setProperty("class", "primary");
    createButton_->setFixedWidth(100);
    createButton_->hide();
    downloadButton_->hide();
    locateButton_->hide();
    importBinariesButton_->hide();

    connect(backButton_, &QPushButton::clicked, this, [this]() { goToStep(currentStep_ - 1); });
    connect(nextButton_, &QPushButton::clicked, this, [this]() { goToStep(currentStep_ + 1); });
    connect(createButton_, &QPushButton::clicked, this, [this]() {
        emit readyToRun(exe_, embedConfig_, configFolder_);
    });

    auto *navSep = new QFrame(this);
    navSep->setFrameShape(QFrame::HLine);
    navSep->setFrameShadow(QFrame::Sunken);

    auto *navRow = new QHBoxLayout;
    navRow->setContentsMargins(8, 4, 8, 6);
    navRow->addStretch(1);
    navRow->addWidget(backButton_);
    navRow->addWidget(nextButton_);
    navRow->addWidget(createButton_);

    // ------------------------------------------------------------------ //
    // Top-level layout
    // ------------------------------------------------------------------ //
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(pages_, 1);
    layout->addWidget(navSep);
    layout->addLayout(navRow);

    probeEmbeddedResources();
    updateNavButtons();
}

// ---------------------------------------------------------------------------
// Wizard navigation
// ---------------------------------------------------------------------------
void ToolEmbedSetupView::goToStep(int step)
{
    if (step < 0 || step > 2)
        return;

    currentStep_ = step;
    pages_->setCurrentIndex(step);

    if (step == 2) {
        // Refresh the files page whenever we arrive on it.
        refreshDownloadsPage();
        refreshCreateButton();
    }

    updateNavButtons();
}

void ToolEmbedSetupView::updateNavButtons()
{
    backButton_->setEnabled(currentStep_ > 0);

    if (currentStep_ < 2) {
        const bool canAdvance = (currentStep_ == 0) ? !exe_.isEmpty() : !embedConfig_.isEmpty();
        nextButton_->setEnabled(canAdvance);
        nextButton_->show();
        createButton_->hide();
    } else {
        nextButton_->hide();
        createButton_->show();
        refreshCreateButton(); // also calls refreshDownloadButton
    }
}

// ---------------------------------------------------------------------------
// Embedded resource probing
// ---------------------------------------------------------------------------
void ToolEmbedSetupView::probeEmbeddedResources()
{
    auto embeddedExe = Resource::GetResourceData(L"RT_RCDATA", L"DFIRORC_EXE", 0);
    if (embeddedExe) {
        if (!QDir().mkpath(tempBaseDir_)) {
            LOG_CRITICAL("Failed to create temp directory: {}", tempBaseDir_.toStdString());
        } else {
            const QString tmpExe = QDir(tempBaseDir_).absoluteFilePath("DFIR-ORC-embedded.exe");
            QFile file(tmpExe);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)
                && file.write(
                       reinterpret_cast<const char *>(embeddedExe->data()),
                       static_cast<qint64>(embeddedExe->size()))
                       == static_cast<qint64>(embeddedExe->size())) {
                exe_ = tmpExe;
            }
        }
    }
    refreshExePage();

    auto extractedConfig = ExtractEmbeddedConfig(tempBaseDir_);
    if (extractedConfig) {
        configFolder_ = *extractedConfig;
        auto embedConfig = FindEmbedConfiguration(configFolder_);
        if (embedConfig)
            embedConfig_ = *embedConfig;
        else
            configFolder_.clear();
        configIsEmbedded_ = !configFolder_.isEmpty();
    }
    refreshConfigPage();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void ToolEmbedSetupView::onChangeExe()
{
    const bool isEmbedded = exe_.contains("DFIR-ORC-embedded.exe");
    const QString initialDir = isEmbedded ? QDir::homePath() : exe_;
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select DFIR-ORC executable"),
        initialDir,
        tr("Executable (*.exe);;All Files (*.*)"));
    if (path.isEmpty())
        return;

    exe_ = path;
    refreshExePage();
    updateNavButtons();
}

void ToolEmbedSetupView::onChangeConfig()
{
    const QString initialDir = configIsEmbedded_ ? QDir::currentPath() : configFolder_;
    const QString folder = QFileDialog::getExistingDirectory(
        this, tr("Select DFIR-ORC configuration folder"), initialDir);
    if (folder.isEmpty())
        return;

    const auto embedConfig = FindEmbedConfiguration(folder);
    if (!embedConfig) {
        QMessageBox::critical(this, tr("Configuration Error"), embedConfig.error());
        return;
    }

    configFolder_ = folder;
    embedConfig_ = *embedConfig;
    configIsEmbedded_ = false;
    refreshConfigPage();
    updateNavButtons();
}

void ToolEmbedSetupView::onImportConfig()
{
    const QString srcExe = QFileDialog::getOpenFileName(
        this,
        tr("Select a configured DFIR-ORC executable"),
        QDir::homePath(),
        tr("Executable (*.exe);;All Files (*.*)"));
    if (srcExe.isEmpty())
        return;

    // Destination: %TEMP%/dfir-orc-forge/<pid>/dump
    const QString dumpDir = tempBaseDir_ + "/dump";
    if (!QDir().mkpath(dumpDir)) {
        QMessageBox::critical(
            this, tr("Import failed"), tr("Failed to create temporary directory:\n%1").arg(dumpDir));
        return;
    }

    auto rv = runToolembedDump(srcExe, dumpDir);
    if (!rv) {
        QMessageBox::critical(
            this,
            tr("Import failed"),
            tr("Failed to dump the configuration from:\n%1\n\n%2")
                .arg(QDir::toNativeSeparators(srcExe), rv.error()));
        return;
    }

    const auto embedConfig = FindEmbedConfiguration(dumpDir);
    if (!embedConfig) {
        QMessageBox::critical(
            this,
            tr("Import failed"),
            tr("No toolembed configuration found in the dump output:\n%1\n\n%2")
                .arg(QDir::toNativeSeparators(dumpDir), embedConfig.error()));
        return;
    }

    configFolder_ = dumpDir;
    embedConfig_ = *embedConfig;
    configIsEmbedded_ = false;
    refreshConfigPage();
    updateNavButtons();
}

void ToolEmbedSetupView::onImportBinaries()
{
    const QString srcExe = QFileDialog::getOpenFileName(
        this,
        tr("Select a configured DFIR-ORC executable"),
        QDir::homePath(),
        tr("Executable (*.exe);;All Files (*.*)"));
    if (srcExe.isEmpty())
        return;

    // Separate sub-directory so it does not collide with config dumps.
    const QString dumpDir = tempBaseDir_ + "/bindump";
    if (!QDir().mkpath(dumpDir)) {
        QMessageBox::critical(
            this, tr("Import failed"), tr("Failed to create temporary directory:\n%1").arg(dumpDir));
        return;
    }

    auto rv = runToolembedDump(srcExe, dumpDir);
    if (!rv) {
        QMessageBox::critical(
            this,
            tr("Import failed"),
            tr("Failed to dump the executable:\n%1\n\n%2")
                .arg(QDir::toNativeSeparators(srcExe), rv.error()));
        return;
    }

    applyLocalDir(dumpDir);
}

// ---------------------------------------------------------------------------
// Page refresh helpers
// ---------------------------------------------------------------------------
void ToolEmbedSetupView::refreshExePage()
{
    if (exe_.isEmpty()) {
        exeIcon_->setText(errIcon());
        exeLabel_->setText(tr("None"));
        exeProductLabel_->hide();
        return;
    }

    const bool isEmbedded = exe_.contains("DFIR-ORC-embedded.exe");
    const auto [name, ver, unused] = peProductInfo(exe_);
    const QString productStr = (!name.isEmpty() && !ver.isEmpty()) ? tr("%1 %2").arg(name, ver)
                               : (!name.isEmpty())                 ? name
                                                                   : ver;

    if (isEmbedded) {
        exeIcon_->setText(okIcon());
        exeLabel_->setText(tr("Embedded"));
        if (!productStr.isEmpty()) {
            exeProductLabel_->setText(productStr);
            exeProductLabel_->show();
        } else {
            exeProductLabel_->hide();
        }
    } else if (productStr.isEmpty()) {
        exeIcon_->setText(errIcon());
        exeLabel_->setText(QDir::toNativeSeparators(exe_));
        exeProductLabel_->setText(tr("No version info -- may not be a DFIR-ORC executable"));
        exeProductLabel_->show();
    } else {
        exeIcon_->setText(okIcon());
        exeLabel_->setText(QDir::toNativeSeparators(exe_));
        exeProductLabel_->setText(productStr);
        exeProductLabel_->show();
    }
}

void ToolEmbedSetupView::refreshConfigPage()
{
    if (configFolder_.isEmpty()) {
        configIcon_->setText(errIcon());
        configLabel_->setText(tr("None"));
        configLabel_->setToolTip(QString());
    } else if (!QDir(configFolder_).exists()) {
        configIcon_->setText(errIcon());
        configLabel_->setText(tr("Missing"));
        configLabel_->setToolTip(QDir::toNativeSeparators(configFolder_));
    } else {
        configIcon_->setText(okIcon());
        if (configIsEmbedded_) {
            configLabel_->setText(tr("Embedded"));
            configLabel_->setToolTip(QString());
        } else {
            configLabel_->setText(QDir::toNativeSeparators(embedConfig_));
            configLabel_->setToolTip(QDir::toNativeSeparators(embedConfig_));
        }
    }
}

void ToolEmbedSetupView::refreshDownloadsPage()
{
    allFiles_.clear();
    downloadsTree_->clear();
    filesStatusLabel_->clear();

    if (!embedConfig_.isEmpty()) {
        auto all = ListEmbedFiles(embedConfig_, configFolder_);
        if (all)
            allFiles_ = std::move(*all);
        else
            filesStatusLabel_->setText(
                tr("<span style='color:#c33'>Failed to read configuration: %1</span>")
                    .arg(all.error()));
    }

    for (const auto &f : allFiles_) {
        auto *item = new QTreeWidgetItem(downloadsTree_);
        item->setText(0, f.name.isEmpty() ? QFileInfo(f.dest).fileName() : f.name);
        item->setText(1, f.urlParsed);
        item->setToolTip(1, f.url);

        if (f.presentOnDisk) {
            item->setText(2, tr("Present"));
            item->setIcon(2, style()->standardIcon(QStyle::SP_DialogApplyButton));
            const QColor grey = palette().color(QPalette::Disabled, QPalette::Text);
            for (int col = 0; col < 3; ++col)
                item->setForeground(col, grey);
        } else {
            item->setText(2, tr("Missing"));
            item->setIcon(2, style()->standardIcon(QStyle::SP_MessageBoxWarning));
        }
    }
    downloadsTree_->resizeColumnToContents(2);
}

void ToolEmbedSetupView::refreshCreateButton()
{
    const bool configured = !exe_.isEmpty() && !embedConfig_.isEmpty();

    int totalMissing = 0;
    int missingWithoutUrl = 0;
    for (const auto &f : allFiles_) {
        if (!f.presentOnDisk) {
            ++totalMissing;
            if (f.urlParsed.isEmpty())
                ++missingWithoutUrl;
        }
    }

    createButton_->setText(tr("Create"));
    createButton_->setEnabled(configured && totalMissing == 0);

    if (missingWithoutUrl > 0) {
        createButton_->setToolTip(
            tr("%1 file(s) are missing and cannot be downloaded automatically."
               " Please provide them manually.")
                .arg(missingWithoutUrl));
        filesStatusLabel_->setText(
            tr("<span style='color:#c33'>%1 file(s) are missing and cannot be "
               "downloaded automatically. Please provide them manually.</span>")
                .arg(missingWithoutUrl));
    } else if (totalMissing > 0) {
        createButton_->setToolTip(
            tr("%1 file(s) are missing. Use Download or Browse directory to resolve them.")
                .arg(totalMissing));
        filesStatusLabel_->clear();
    } else {
        createButton_->setToolTip(QString());
        filesStatusLabel_->clear();
    }

    refreshDownloadButton();
}

void ToolEmbedSetupView::refreshDownloadButton()
{
    int missingWithUrl = 0;
    int missingTotal = 0;
    for (const auto &f : allFiles_) {
        if (!f.presentOnDisk) {
            ++missingTotal;
            if (!f.urlParsed.isEmpty())
                ++missingWithUrl;
        }
    }

    if (missingWithUrl > 0) {
        downloadButton_->setText(tr("Download (%1)").arg(missingWithUrl));
        downloadButton_->show();
    } else {
        downloadButton_->hide();
    }

    if (missingTotal > 0) {
        locateButton_->show();
        importBinariesButton_->show();
    } else {
        locateButton_->hide();
        importBinariesButton_->hide();
    }
}

// ---------------------------------------------------------------------------
// Dump helper
// ---------------------------------------------------------------------------
std::expected<QString, QString> ToolEmbedSetupView::runToolembedDump(
    const QString &srcExe, const QString &destDir)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    QProcess proc;
    proc.setProgram(srcExe);
    proc.setArguments(
        {QStringLiteral("toolembed"),
         QStringLiteral("/dump"),
         QStringLiteral("/out=") + QDir::toNativeSeparators(destDir)});
    proc.start();

    if (!proc.waitForStarted(5000)) {
        QApplication::restoreOverrideCursor();
        return std::unexpected(tr("Failed to start process: %1").arg(proc.errorString()));
    }

    const bool finished = proc.waitForFinished(60000);

    QApplication::restoreOverrideCursor();

    if (!finished || proc.exitCode() != 0) {
        const QString errMsg = QString::fromLocal8Bit(proc.readAllStandardError());
        return std::unexpected(
            errMsg.isEmpty() ? tr("The process did not complete successfully.") : errMsg);
    }
    return destDir;
}

// ---------------------------------------------------------------------------
// Local directory lookup
// ---------------------------------------------------------------------------
bool ToolEmbedSetupView::installFromLocalDir(const EmbedFile &f)
{
    if (localDir_.isEmpty())
        return false;

    const QString wantedBasename = QFileInfo(f.dest).fileName().toLower();
    QDirIterator
        it(localDir_,
           QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
           QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString candidate = it.next();
        if (QFileInfo(candidate).fileName().toLower() != wantedBasename)
            continue;

        const QString destDir = QFileInfo(f.dest).absolutePath();
        if (!QDir().mkpath(destDir))
            return false;

        QFile::remove(f.dest);
        return QFile::copy(candidate, f.dest);
    }
    return false;
}

void ToolEmbedSetupView::onBrowseLocalDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select directory containing binaries"), localDir_);
    if (dir.isEmpty())
        return;

    applyLocalDir(dir);
}

void ToolEmbedSetupView::applyLocalDir(const QString &dir)
{
    localDir_ = dir;

    int found = 0;
    int missing = 0;
    for (const auto &f : allFiles_) {
        if (f.presentOnDisk)
            continue;
        if (installFromLocalDir(f))
            ++found;
        else
            ++missing;
    }

    refreshDownloadsPage();
    refreshCreateButton();

    if (found == 0) {
        QMessageBox::information(
            this,
            tr("No files found"),
            tr("No matching files were found in the selected directory."));
    } else if (missing > 0) {
        QMessageBox::information(
            this,
            tr("Partial match"),
            tr("%1 file(s) installed from directory; %2 still missing.").arg(found).arg(missing));
    }
}

// ---------------------------------------------------------------------------
// Drag-and-drop
// ---------------------------------------------------------------------------
void ToolEmbedSetupView::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (!mime->hasUrls() || mime->urls().size() != 1) {
        event->ignore();
        return;
    }

    const QUrl url = mime->urls().first();
    if (!url.isLocalFile()) {
        event->ignore();
        return;
    }

    const QString path = url.toLocalFile();
    const QFileInfo fi(path);

    switch (currentStep_) {
    case 0:
        // Accept a .exe file
        if (fi.isFile() && fi.suffix().compare("exe", Qt::CaseInsensitive) == 0)
            event->acceptProposedAction();
        else
            event->ignore();
        break;
    case 1:
    case 2:
        // Accept a directory
        if (fi.isDir())
            event->acceptProposedAction();
        else
            event->ignore();
        break;
    default:
        event->ignore();
        break;
    }
}

void ToolEmbedSetupView::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (!mime->hasUrls() || mime->urls().size() != 1)
        return;

    const QString path = mime->urls().first().toLocalFile();
    if (path.isEmpty())
        return;

    const QFileInfo fi(path);

    switch (currentStep_) {
    case 0: {
        if (!fi.isFile())
            return;
        exe_ = path;
        refreshExePage();
        updateNavButtons();
        event->acceptProposedAction();
        break;
    }
    case 1: {
        if (!fi.isDir())
            return;
        const auto embedConfig = FindEmbedConfiguration(path);
        if (!embedConfig) {
            QMessageBox::critical(this, tr("Configuration Error"), embedConfig.error());
            return;
        }
        configFolder_ = path;
        embedConfig_ = *embedConfig;
        configIsEmbedded_ = false;
        refreshConfigPage();
        updateNavButtons();
        event->acceptProposedAction();
        break;
    }
    case 2: {
        if (!fi.isDir())
            return;
        applyLocalDir(path);
        event->acceptProposedAction();
        break;
    }
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// EULA prompt factory
// ---------------------------------------------------------------------------
EulaPromptFn ToolEmbedSetupView::makeEulaPrompt()
{
    return [this](const QString &eulaText, const QString &executableName) -> bool {
        QDialog dlg(this);
        dlg.setWindowTitle(
            executableName.isEmpty() ? tr("License Agreement")
                                     : tr("License Agreement -- %1").arg(executableName));
        auto *layout = new QVBoxLayout(&dlg);
        auto *edit = new QTextEdit(&dlg);
        edit->setReadOnly(true);
        edit->setPlainText(eulaText);
        layout->addWidget(edit);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("Accept"));
        buttons->button(QDialogButtonBox::Cancel)->setText(tr("Decline"));
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        dlg.resize(640, 480);
        return dlg.exec() == QDialog::Accepted;
    };
}

// ---------------------------------------------------------------------------
// Context menu on the downloads tree
// ---------------------------------------------------------------------------
void ToolEmbedSetupView::onTreeContextMenu(const QPoint &pos)
{
    auto *item = downloadsTree_->itemAt(pos);
    if (!item)
        return;

    const int row = downloadsTree_->indexOfTopLevelItem(item);
    if (row < 0 || row >= allFiles_.size())
        return;

    const EmbedFile &f = allFiles_.at(row);

    QMenu menu(this);
    QAction *actRedownload = nullptr;
    QAction *actCopyUrl = nullptr;
    QAction *actLoadFromDir = nullptr;

    if (!f.urlParsed.isEmpty()) {
        actRedownload = menu.addAction(
            style()->standardIcon(QStyle::SP_BrowserReload),
            f.presentOnDisk ? tr("Re-download and overwrite") : tr("Download"));
        actCopyUrl
            = menu.addAction(style()->standardIcon(QStyle::SP_FileIcon), tr("Copy source URL"));
    }

    actLoadFromDir
        = menu.addAction(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Load from directory..."));

    QAction *chosen = menu.exec(downloadsTree_->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (actRedownload && chosen == actRedownload) {
        if (auto result = ResolveEmbedDownloads(
                embedConfig_, configFolder_, makeEulaPrompt(), QSet<QString>{f.dest});
            !result) {
            QMessageBox::critical(this, tr("Download failed"), result.error());
        }
        refreshDownloadsPage();
        refreshCreateButton();
    } else if (actCopyUrl && chosen == actCopyUrl) {
        QApplication::clipboard()->setText(f.urlParsed);
    } else if (actLoadFromDir && chosen == actLoadFromDir) {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select directory containing %1").arg(QFileInfo(f.dest).fileName()), localDir_);
        if (dir.isEmpty())
            return;

        localDir_ = dir;
        if (!installFromLocalDir(f)) {
            QMessageBox::warning(
                this,
                tr("File not found"),
                tr("'%1' was not found in the selected directory.")
                    .arg(QFileInfo(f.dest).fileName()));
        }
        refreshDownloadsPage();
        refreshCreateButton();
    }
}
