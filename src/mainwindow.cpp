//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "mainwindow.h"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <windows.h>
#include <shellapi.h>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTextEdit>
#include <QUrl>

#include "mainwindow.moc"

#include "editor/editorhub.h"
#include "editor/resourceeditor.h"
#include "formatter/formatter.h"
#include "resource/resource.h"
#include "textedit/texteditwidget.h"
#include "toolembed/toolembedview.h"
#include "utils/log.h"
#include "utils/peinfo.h"

#include "toolembed/embedconfiguration.h"
#include "xsdstore.h"

#include "7z/extract.h"

#include "version.h"

namespace {

QString MakeWindowTitle(QString base, std::optional<QString> path, bool modified)
{
    if (path) {
        base += " - ";
    }

    if (modified) {
        return QString("%1%2*").arg(base).arg(path.value_or(""));
    } else {
        return QString("%1%2").arg(base).arg(path.value_or(""));
    }
}

QString MakeNewXmlConfiguration(std::string_view command)
{
    auto xml
        = std::format("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n\n<{}>\n</{}>", command, command);
    return QString::fromUtf8(xml);
}

QString GetNewXmlConfiguration(std::string_view command)
{
    std::wstring name(std::cbegin(command), std::cend(command));

    if (name == L"dfir-orc") {
        name = L"local";
    }

    if (name == L"wolf") {
        name = L"wolflauncher";
    }

    auto data
        = Resource::GetResourceData(L"RT_RCDATA", std::format(L"CONFIGURATION_NEW_{}", name), 0);
    if (!data) {
        return MakeNewXmlConfiguration(command);
    }

    return QString::fromUtf8(*data);
}

// The documentation is optional: it is embedded post-build as a zip archive
// whose entries live under 'docs/'. Return its data only when the resource is
// present and actually looks like a zip archive.
std::optional<std::vector<uint8_t>> GetEmbeddedDocumentation()
{
    auto data = Resource::GetResourceData(L"RT_RCDATA", L"DFIRORC_DOC", 0);
    if (!data) {
        return {};
    }

    constexpr std::array<uint8_t, 4> kZipLocalFileHeader = {'P', 'K', 0x03, 0x04};
    if (data->size() < kZipLocalFileHeader.size()
        || !std::equal(
            std::cbegin(kZipLocalFileHeader), std::cend(kZipLocalFileHeader), std::cbegin(*data))) {
        LOG_WARN("Embedded documentation is not a zip archive, ignoring it");
        return {};
    }

    return std::move(*data);
}

QString GetNewYaraRule()
{
    auto data = Resource::GetResourceData(L"RT_RCDATA", L"CONFIGURATION_NEW_YARA_RULE", 0);
    if (!data) {
        return {};
    }

    return QString::fromUtf8(*data);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , xsdStore_(std::make_unique<XsdStore>())
{
    setupUI();

    const QList<QPair<QAction *, QString>> xmlActions = {
        {actionNewXmlFastFind_, "fastfind"},
        {actionNewXmlFatInfo_, "fatinfo"},
        {actionNewXmlGetSamples_, "getsamples"},
        {actionNewXmlGetSectors_, "getsectors"},
        {actionNewXmlGetThis_, "getthis"},
        {actionNewXmlLocal_, "dfir-orc"},
        {actionNewXmlNtfsInfo_, "ntfsinfo"},
        {actionNewXmlObjInfo_, "objinfo"},
        {actionNewXmlRegInfo_, "reginfo"},
        {actionNewXmlUsnInfo_, "usninfo"},
        {actionNewXmlWolf_, "wolf"},
    };

    for (const auto &[action, command] : xmlActions) {
        connect(action, &QAction::triggered, this, [this, command]() {
            newFile(GetNewXmlConfiguration(command.toStdString()), EditorFormat::XmlConfiguration);
        });
    }

    connect(actionNewYaraRule_, &QAction::triggered, [this]() {
        newFile(GetNewYaraRule(), EditorFormat::YaraRule);
    });

    connect(actionNewOrc_, &QAction::triggered, [this]() {
        setCentralWidget(nullptr);
        path_.reset();
        setModifiedStatus(false);

        auto *embedView = new ToolEmbedView(this);
        connect(embedView, &ToolEmbedView::fileReady, this, &MainWindow::loadFile);
        setCentralWidget(embedView);
        embedView->promptAndRun();
    });

    connect(actionOpen_, &QAction::triggered, this, &MainWindow::openFileDialog);
    connect(actionSave_, &QAction::triggered, this, &MainWindow::saveFile);
    connect(actionSaveAs_, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(actionExit_, &QAction::triggered, this, &MainWindow::close);

    if (actionViewHelp_) {
        connect(actionViewHelp_, &QAction::triggered, this, &MainWindow::help);
    }

    connect(actionViewOnlineHelp_, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("https://dfir-orc.github.io"));
    });

    connect(actionAbout_, &QAction::triggered, this, &MainWindow::about);

    connect(actionFind_, &QAction::triggered, this, [this]() {
        sendEditKey(Qt::Key_F, Qt::ControlModifier);
    });
    connect(actionGotoLine_, &QAction::triggered, this, [this]() {
        sendEditKey(Qt::Key_G, Qt::ControlModifier);
    });
    connect(actionToggleComment_, &QAction::triggered, this, [this]() {
        sendEditKey(Qt::Key_Slash, Qt::ControlModifier);
    });
    connect(actionMoveLineUp_, &QAction::triggered, this, [this]() {
        sendEditKey(Qt::Key_Up, Qt::AltModifier);
    });
    connect(actionMoveLineDown_, &QAction::triggered, this, [this]() {
        sendEditKey(Qt::Key_Down, Qt::AltModifier);
    });
    connect(actionWordWrap_, &QAction::triggered, this, [this](bool checked) {
        if (auto *tw = activeTextEditWidget()) {
            tw->setWordWrap(checked);
        }
    });

    xsdStore_->loadFromResources();

    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        updateEditActions(now);
    });

#ifndef _DEBUG
    actionReloadXsd_->setVisible(false);
#else
    connect(actionReloadXsd_, &QAction::triggered, [&]() {
        xsdStore_->Clear();
        auto xsdPath = std::filesystem::path(__FILE__).parent_path() / ".." / "assets" / "xsd";
        xsdStore_->loadFromDirectory(QString::fromWCharArray(xsdPath.c_str()));
    });
#endif
}

MainWindow::~MainWindow() {}

void MainWindow::updateEditActions(QWidget *focused)
{
    auto *textWidget = activeTextEditWidget();
    const bool active = textWidget != nullptr;

    const QList<QAction *> editActions
        = {actionFind_,
           actionGotoLine_,
           actionToggleComment_,
           actionMoveLineUp_,
           actionMoveLineDown_,
           actionWordWrap_};
    for (auto *action : editActions) {
        action->setEnabled(active);
    }

    if (active) {
        actionWordWrap_->setChecked(textWidget->wordWrap());
    }
}

TextEditWidget *MainWindow::activeTextEditWidget() const
{
    auto *focused = QApplication::focusWidget();
    if (!focused || !qobject_cast<QTextEdit *>(focused)) {
        return nullptr;
    }

    auto *central = centralWidget();
    QWidget *w = focused->parentWidget();
    while (w) {
        if (auto *tw = qobject_cast<TextEditWidget *>(w)) {
            // Confirm it lives inside the current central widget.
            QWidget *inner = tw;
            while (inner) {
                if (inner == central) {
                    return tw;
                }
                inner = inner->parentWidget();
            }
            return nullptr;
        }
        w = w->parentWidget();
    }
    return nullptr;
}

void MainWindow::sendEditKey(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    auto *focused = QApplication::focusWidget();
    if (!focused) {
        return;
    }

    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(focused, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QCoreApplication::sendEvent(focused, &release);
}

void MainWindow::setupUI()
{
    windowTitle_ = "DFIR-ORC Forge";
    setWindowTitle(windowTitle_);

    actionOpen_ = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen), "&Open", this);
    actionOpen_->setShortcut(QKeySequence::Open);

    actionSave_ = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSave), "&Save", this);
    actionSave_->setShortcut(QKeySequence::Save);
    actionSave_->setEnabled(false);

    actionSaveAs_
        = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSaveAs), "Save &As", this);
    actionSaveAs_->setShortcut(QKeySequence::SaveAs);
    actionSaveAs_->setEnabled(false);

    actionExit_ = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::WindowClose), "E&xit", this);
    actionExit_->setMenuRole(QAction::MenuRole::QuitRole);
    actionExit_->setShortcut(QKeySequence::Quit);

    actionReloadXsd_ = new QAction("&Reload XSD", this);

    actionNewOrc_ = new QAction("New &DFIR-ORC", this);
    actionNewOrc_->setShortcut(QKeySequence::New);

    actionNewXmlFastFind_ = new QAction("&FastFind", this);
    actionNewXmlFatInfo_ = new QAction("F&atInfo", this);
    actionNewXmlGetSamples_ = new QAction("Get&Samples", this);
    actionNewXmlGetSectors_ = new QAction("GetS&ectors", this);
    actionNewXmlGetThis_ = new QAction("Get&This", this);
    actionNewXmlNtfsInfo_ = new QAction("&NtfsInfo", this);
    actionNewXmlObjInfo_ = new QAction("&ObjInfo", this);
    actionNewXmlRegInfo_ = new QAction("&RegInfo", this);
    actionNewXmlUsnInfo_ = new QAction("&UsnInfo", this);

    auto actionSeparator = new QAction();
    actionSeparator->setSeparator(true);

    actionNewXmlLocal_ = new QAction("&Local", this);
    actionNewXmlWolf_ = new QAction("&WolfLauncher", this);

    actionNewYaraRule_ = new QAction("New &Yara rule", this);

    xmlConfigurationMenu_ = new QMenu("New &XML configuration");
    xmlConfigurationMenu_->addActions(
        {actionNewXmlFastFind_,
         actionNewXmlFatInfo_,
         actionNewXmlGetSamples_,
         actionNewXmlGetSectors_,
         actionNewXmlGetThis_,
         actionNewXmlNtfsInfo_,
         actionNewXmlObjInfo_,
         actionNewXmlRegInfo_,
         actionNewXmlUsnInfo_,
         actionSeparator,
         actionNewXmlLocal_,
         actionNewXmlWolf_});

    fileMenu_ = new QMenu("&File", this);
    fileMenu_->addAction(actionNewOrc_);
    fileMenu_->addMenu(xmlConfigurationMenu_);
    fileMenu_->addActions(
        {actionNewYaraRule_, actionOpen_, actionSave_, actionSaveAs_, actionExit_, actionReloadXsd_});

    for (auto *action : fileMenu_->actions()) {
        action->setShortcutVisibleInContextMenu(true);
        action->setIconVisibleInMenu(false);
    }

    actionFind_ = new QAction("&Find", this);
    actionFind_->setShortcut(QKeySequence::Find);
    actionFind_->setShortcutContext(Qt::WidgetShortcut);
    actionFind_->setEnabled(false);

    actionGotoLine_ = new QAction("&Go to Line", this);
    actionGotoLine_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    actionGotoLine_->setShortcutContext(Qt::WidgetShortcut);
    actionGotoLine_->setEnabled(false);

    actionToggleComment_ = new QAction("&Toggle Line Comment", this);
    actionToggleComment_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));
    actionToggleComment_->setShortcutContext(Qt::WidgetShortcut);
    actionToggleComment_->setEnabled(false);

    actionMoveLineUp_ = new QAction("Move Line &Up", this);
    actionMoveLineUp_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    actionMoveLineUp_->setShortcutContext(Qt::WidgetShortcut);
    actionMoveLineUp_->setEnabled(false);

    actionMoveLineDown_ = new QAction("Move Line &Down", this);
    actionMoveLineDown_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Down));
    actionMoveLineDown_->setShortcutContext(Qt::WidgetShortcut);
    actionMoveLineDown_->setEnabled(false);

    // QTextEdit defaults to WrapAtWordBoundaryOrAnywhere, so word wrap starts on.
    actionWordWrap_ = new QAction("&Word Wrap", this);
    actionWordWrap_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Z));
    actionWordWrap_->setShortcutContext(Qt::WidgetShortcut);
    actionWordWrap_->setCheckable(true);
    actionWordWrap_->setChecked(true);
    actionWordWrap_->setEnabled(false);

    auto *editSeparator = new QAction(this);
    editSeparator->setSeparator(true);

    editMenu_ = new QMenu("&Edit", this);
    editMenu_->addActions(
        {actionFind_,
         actionGotoLine_,
         actionToggleComment_,
         actionMoveLineUp_,
         actionMoveLineDown_,
         editSeparator,
         actionWordWrap_});

    for (auto *action : editMenu_->actions()) {
        action->setShortcutVisibleInContextMenu(true);
        action->setIconVisibleInMenu(false);
    }

    // Only offer the local help when usable documentation is embedded
    actionViewHelp_ = nullptr;
    if (GetEmbeddedDocumentation()) {
        actionViewHelp_
            = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::HelpFaq), "&View Help", this);
        actionViewHelp_->setShortcut(QKeySequence::HelpContents);
    }

    actionViewOnlineHelp_
        = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::InsertLink), "View &Online Help", this);
    actionViewOnlineHelp_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F1));
    actionAbout_ = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::HelpAbout), "&About", this);

    QList<QAction *> helpActions;
    if (actionViewHelp_) {
        helpActions.append(actionViewHelp_);
    }
    helpActions.append({actionViewOnlineHelp_, actionAbout_});
    for (auto *action : helpActions) {
        action->setShortcutVisibleInContextMenu(true);
        action->setIconVisibleInMenu(false);
    }

    helpMenu_ = new QMenu("&Help", this);
    helpMenu_->addActions(helpActions);

    menuBar_ = new QMenuBar(this);
    menuBar_->insertMenu(nullptr, fileMenu_);
    menuBar_->insertMenu(nullptr, editMenu_);
    menuBar_->insertMenu(nullptr, helpMenu_);

    auto *widget = new QWidget(this);
    widget->setEnabled(true);
    setCentralWidget(widget);

    setMenuBar(menuBar_);

    resize(1197, 751);
    setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonTextOnly);
    setAnimated(false);
    setDocumentMode(false);

    //retranslateUi(this);

    setContentsMargins(3, 3, 3, 3);
}

void MainWindow::newFile(QString content, EditorFormat format)
{
    if (hasModifiedStatus()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Confirm action",
            "You have unsaved changes. Do you really want to close edition?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    setCentralWidget(nullptr);
    path_.reset();
    setModifiedStatus(true);

    auto editor = new EditionWidget(*xsdStore_, this);
    connect(&editor->editorSignals(), &EditorInterface::Signals::contentsChanged, this, [this]() {
        setModifiedStatus(true);
    });

    if (!editor->load(content.toUtf8(), format)) {
        LOG_CRITICAL("Failed to initialize editor");
        editor->deleteLater();
        return;
    }

    setCentralWidget(editor);
}

void MainWindow::openFileDialog()
{
    if (hasModifiedStatus()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Confirm action",
            "You have unsaved changes. Do you really want to close edition?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    auto path
        = QFileDialog::getOpenFileName(this, "Open File", "", "Supported Files (*.exe *.xml *.yara)");
    if (path.isEmpty()) {
        return;
    }

    loadFile(path);
}

void MainWindow::loadFile(QString path)
{
    path_ = path;

    setCentralWidget(nullptr);
    setModifiedStatus(false);

    QFile filePath(path);
    if (!filePath.exists()) {
        LOG_DEBUG(L"File does not exists (path: {})", path);
        return;
    }

    bool loaded = false;
    if (path_->endsWith(".exe")) {
        auto editor = new ResourceEditor(*xsdStore_, this);
        if (!editor->load(*path_)) {
            editor->deleteLater();
            return;
        }

        connect(&editor->editorSignals(), &EditorSignals::contentsChanged, this, [this]() {
            setModifiedStatus(true);
        });

        connect(&editor->editorSignals(), &EditorSignals::fileSaved, this, [this]() {
            setModifiedStatus(false);
        });

        setCentralWidget(editor);
    } else {
        auto editor = new EditionWidget(*xsdStore_, this);
        if (!editor->load(*path_)) {
            editor->deleteLater();
            return;
        }

        connect(&editor->editorSignals(), &EditorSignals::contentsChanged, this, [this]() {
            setModifiedStatus(true);
        });

        connect(&editor->editorSignals(), &EditorSignals::fileSaved, this, [this]() {
            setModifiedStatus(false);
        });

        setCentralWidget(editor);
    }

    setWindowTitle(MakeWindowTitle(windowTitle_, path_, false));
}

void MainWindow::saveFile()
{
    auto edition = dynamic_cast<EditorInterface *>(centralWidget());
    if (!edition) {
        return;
    }

    auto status = edition->save();
    if (!status) {
        if (edition->path()) {
            LOG_CRITICAL(L"Failed to save {}", edition->path()->toStdWString());
        } else {
            LOG_CRITICAL("Failed to save");
        }

        return;
    }

    setModifiedStatus(false);
    return;
}

void MainWindow::saveFileAs()
{
    auto edition = dynamic_cast<EditorInterface *>(centralWidget());
    if (!edition) {
        return;
    }

    auto status = edition->saveAs();
    if (!status) {
        if (edition->path()) {
            LOG_CRITICAL(L"Failed to save as {}", edition->path()->toStdWString());
        } else {
            LOG_CRITICAL("Failed to save as");
        }

        return;
    }

    setModifiedStatus(false);
    return;
}

void MainWindow::help()
{
    QString tempDirPath = QDir::toNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/dfir-orc-forge");
    QDir tempDir(tempDirPath);
    if (!tempDir.makeAbsolute()) {
        return;
    }

    auto buffer = GetEmbeddedDocumentation();
    if (!buffer) {
        LOG_CRITICAL("No usable documentation is embedded in this build");
        reportHelpFailure();
        return;
    }

    auto rv = SevenZip::ExtractZip(
        std::string_view(reinterpret_cast<const char *>(buffer->data()), buffer->size()),
        tempDir.absolutePath().toStdWString());
    if (!rv) {
        LOG_CRITICAL("Failed to extract help files: {}", rv.error().message());
        reportHelpFailure();
        return;
    }

    const QString indexPath = QDir(tempDir.filePath("docs")).absoluteFilePath("index.html");
    if (!QFileInfo::exists(indexPath)) {
        LOG_CRITICAL(L"Missing help index: {}", indexPath.toStdWString());
        reportHelpFailure();
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(indexPath))) {
        LOG_CRITICAL(L"Failed to open help index: {}", indexPath.toStdWString());
        reportHelpFailure();
        return;
    }
}

void MainWindow::reportHelpFailure()
{
    QMessageBox::warning(
        this,
        tr("Help"),
        tr("The local documentation could not be opened. Use 'View Online Help' instead."));
}

void MainWindow::about()
{
    QDialog aboutDialog(this);
    aboutDialog.setWindowTitle(tr("About"));
    auto *mainLayout = new QVBoxLayout(&aboutDialog);

    // --- Upper section: logo + version ---
    {
        QPixmap logo;
        auto res = Resource::GetResourceData(L"RT_RCDATA", L"LOGO", 0);
        if (res && logo.loadFromData(res->data(), res->size(), "PNG")) {
            auto *logoLabel = new QLabel(&aboutDialog);
            logoLabel->setPixmap(logo.scaled(100, 100, Qt::KeepAspectRatio));
            mainLayout->addWidget(logoLabel, 0, Qt::AlignCenter);
        }

        auto *textLabel = new QLabel(&aboutDialog);
        textLabel->setText(QString("<h2>DFIR-ORC Forge</h2>"
                                   "<p>Version %1</p>"
                                   "<p>Configuration editor for DFIR-ORC</p>"
                                   "<p>Copyright &copy; 2025 ANSSI</p>")
                               .arg(kDofVersionString));
        textLabel->setWordWrap(true);
        mainLayout->addWidget(textLabel, 0, Qt::AlignCenter);
    }

    mainLayout->addSpacing(12);

    // Separator
    {
        auto *sep = new QFrame(&aboutDialog);
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        mainLayout->addWidget(sep);
    }

    mainLayout->addSpacing(10);

    // --- Lower section: embedded resources ---
    {
        auto *embedTitle = new QLabel(tr("<b>Embedded resources</b>"), &aboutDialog);
        mainLayout->addWidget(embedTitle);
        mainLayout->addSpacing(8);

        // Collect exe info (icon + text) before building widgets
        QPixmap exeIcon;
        QString exeText;
        QString configCommit;
        QString docCommit;

        // DFIRORC_EXE: write to a fixed-name .exe temp path so the extension is
        // guaranteed, then use ExtractIconExW (reads PE resources directly, no
        // Shell COM needed) for the icon and GetFileVersionInfo for product info.
        {
            auto embeddedExe = Resource::GetResourceData(L"RT_RCDATA", L"DFIRORC_EXE", 0);
            if (embeddedExe) {
                const QString tmpPath = QDir::tempPath() + "/dfir-orc-forge-about.exe";

                QFile tmpFile(tmpPath);
                const bool written = tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
                                     && tmpFile.write(
                                            reinterpret_cast<const char *>(embeddedExe->data()),
                                            static_cast<qint64>(embeddedExe->size()))
                                            == static_cast<qint64>(embeddedExe->size());
                tmpFile.close();

                if (written) {
                    // Icon: extract directly from PE resource group, no Shell needed
                    HICON hLarge = nullptr;
                    if (ExtractIconExW(tmpPath.toStdWString().c_str(), 0, &hLarge, nullptr, 1) == 1
                        && hLarge) {
                        exeIcon = QPixmap::fromImage(QImage::fromHICON(hLarge));
                        DestroyIcon(hLarge);
                    }

                    // Version info
                    auto [name, ver, unused] = peProductInfo(tmpPath);
                    QFile::remove(tmpPath);

                    if (!name.isEmpty() && !ver.isEmpty())
                        exeText = tr("%1 %2").arg(name, ver);
                    else if (!name.isEmpty())
                        exeText = name;
                    else
                        exeText = ver;
                } else {
                    QFile::remove(tmpPath);
                }

                if (exeText.isEmpty())
                    exeText = tr("(no version info)");
            } else {
                exeText = tr("Not embedded");
            }
        }

        // DFIRORC_CONFIG_COMMIT
        {
            auto res = Resource::GetResourceData(L"RT_RCDATA", L"DFIRORC_CONFIG_COMMIT", 0);
            configCommit = res ? QString::fromUtf8(
                                     reinterpret_cast<const char *>(res->data()),
                                     static_cast<int>(res->size()))
                                     .trimmed()
                               : tr("Not embedded");
        }

        // DFIRORC_DOC_COMMIT
        {
            auto res = Resource::GetResourceData(L"RT_RCDATA", L"DFIRORC_DOC_COMMIT", 0);
            docCommit = res ? QString::fromUtf8(
                                  reinterpret_cast<const char *>(res->data()),
                                  static_cast<int>(res->size()))
                                  .trimmed()
                            : tr("Not embedded");
        }

        // Layout: [icon] | grid of 3 label/value rows
        // The icon is left-aligned and spans the full height of the grid.
        auto *hLayout = new QHBoxLayout;
        hLayout->setContentsMargins(0, 0, 0, 0);
        hLayout->setSpacing(12);

        // Icon column -- vertically centered across the three rows.
        // Scale the pixmap to at most 24x24 so it is never clipped, then let
        // the label size itself to the scaled pixmap (no fixed dimensions).
        auto *iconLabel = new QLabel(&aboutDialog);
        iconLabel->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        if (!exeIcon.isNull()) {
            iconLabel->setPixmap(
                exeIcon.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            // Reserve the same width even when there is no icon so the grid
            // columns don't shift depending on whether an exe is embedded.
            iconLabel->setFixedWidth(48);
        }
        hLayout->addWidget(iconLabel, 0, Qt::AlignVCenter);

        // Label/value grid
        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(6);
        grid->setColumnStretch(1, 1);

        auto addGridRow = [&](int row, const QString &label, const QString &value) {
            auto *lbl = new QLabel(label, &aboutDialog);
            lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            auto *val = new QLabel(value, &aboutDialog);
            val->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            val->setWordWrap(true);
            grid->addWidget(lbl, row, 0);
            grid->addWidget(val, row, 1);
        };

        addGridRow(0, tr("DFIR-ORC exe:"), exeText);
        addGridRow(1, tr("DFIR-ORC config:"), configCommit);
        addGridRow(2, tr("DFIR-ORC doc:"), docCommit);

        hLayout->addLayout(grid, 1);
        mainLayout->addLayout(hLayout);
    }

    mainLayout->addSpacing(8);
    aboutDialog.setMinimumWidth(480);
    aboutDialog.exec();
}

bool MainWindow::hasModifiedStatus() const
{
    return actionSave_->isEnabled();
}

void MainWindow::setModifiedStatus(bool value)
{
    std::optional<QString> path;
    auto edition = dynamic_cast<EditorInterface *>(centralWidget());
    if (edition) {
        path = edition->path();
    }

    actionSave_->setEnabled(value);
    actionSaveAs_->setEnabled(value);
    setWindowTitle(MakeWindowTitle(windowTitle_, path, value));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!hasModifiedStatus()) {
        event->accept();
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm action",
        "You have unsaved changes. Do you really want to exit?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
}
