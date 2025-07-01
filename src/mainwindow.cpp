//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "mainwindow.h"

#include <format>
#include <string>
#include <system_error>

#include <QByteArray>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>

#include "mainwindow.moc"

#include "editor/editorhub.h"
#include "editor/resourceeditor.h"
#include "formatter/formatter.h"
#include "resource/resource.h"
#include "resource/resourcedirectory.h"
#include "utils/log.h"

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

    connect(actionOpen_, &QAction::triggered, this, &MainWindow::openFileDialog);
    connect(actionSave_, &QAction::triggered, this, &MainWindow::saveFile);
    connect(actionSaveAs_, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(actionExit_, &QAction::triggered, this, &MainWindow::close);

    connect(actionViewHelp_, &QAction::triggered, this, &MainWindow::help);
    connect(actionViewOnlineHelp_, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("https://dfir-orc.github.io"));
    });

    connect(actionAbout_, &QAction::triggered, this, &MainWindow::about);

    xsdStore_->loadFromResources();

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

void MainWindow::setupUI()
{
    windowTitle_ = "DFIR-ORC Forge";
    setWindowTitle(windowTitle_);

    actionOpen_ = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen), "Open", this);
    actionOpen_->setShortcut(QKeySequence::Open);

    actionSave_ = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSave), "Save", this);
    actionSave_->setShortcut(QKeySequence::Save);
    actionSave_->setEnabled(false);

    actionSaveAs_ = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSaveAs), "Save As", this);
    actionSaveAs_->setShortcut(QKeySequence::SaveAs);
    actionSaveAs_->setEnabled(false);

    actionExit_ = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::WindowClose), "Exit", this);
    actionExit_->setMenuRole(QAction::MenuRole::QuitRole);
    actionExit_->setShortcut(QKeySequence::Quit);

    actionReloadXsd_ = new QAction("Reload XSD", this);

    actionNewXmlFastFind_ = new QAction("FastFind", this);
    actionNewXmlFatInfo_ = new QAction("FatInfo", this);
    actionNewXmlGetSamples_ = new QAction("GetSamples", this);
    actionNewXmlGetSectors_ = new QAction("GetSectors", this);
    actionNewXmlGetThis_ = new QAction("GetThis", this);
    actionNewXmlNtfsInfo_ = new QAction("NtfsInfo", this);
    actionNewXmlObjInfo_ = new QAction("ObjInfo", this);
    actionNewXmlRegInfo_ = new QAction("RegInfo", this);
    actionNewXmlUsnInfo_ = new QAction("UsnInfo", this);

    auto actionSeparator = new QAction();
    actionSeparator->setSeparator(true);

    actionNewXmlLocal_ = new QAction("Local", this);
    actionNewXmlWolf_ = new QAction("WolfLauncher", this);

    actionNewYaraRule_ = new QAction("New Yara rule", this);

    xmlConfigurationMenu_ = new QMenu("New XML configuration");
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

    fileMenu_ = new QMenu("File", this);
    fileMenu_->addMenu(xmlConfigurationMenu_);
    fileMenu_->addActions(
        {actionNewYaraRule_, actionOpen_, actionSave_, actionSaveAs_, actionExit_, actionReloadXsd_});

    for (auto *action : fileMenu_->actions()) {
        action->setShortcutVisibleInContextMenu(true);
        action->setIconVisibleInMenu(false);
    }

    actionViewHelp_ = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::HelpFaq), "View Help", this);
    actionViewHelp_->setShortcut(QKeySequence::HelpContents);
    actionViewOnlineHelp_
        = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::InsertLink), "View Online Help", this);
    actionAbout_ = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::HelpAbout), "About", this);

    QList<QAction *> helpActions = {actionViewHelp_, actionViewOnlineHelp_, actionAbout_};
    for (auto *action : helpActions) {
        action->setShortcutVisibleInContextMenu(true);
        action->setIconVisibleInMenu(false);
    }

    helpMenu_ = new QMenu("Help", this);
    helpMenu_->addActions(helpActions);

    menuBar_ = new QMenuBar(this);
    menuBar_->insertMenu(nullptr, fileMenu_);
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

    ResourceDirectory resources;
    auto buffer = Resource::GetResourceData(std::wstring(L"RT_RCDATA"), std::wstring(L"DOC"), 0);
    if (!buffer) {
        return;
    }

    auto rv = SevenZip::ExtractZip(
        std::string_view(reinterpret_cast<const char *>(buffer->data()), buffer->size()),
        tempDir.absolutePath().toStdWString());
    if (!rv) {
        LOG_CRITICAL("Failed to extract help files: {}", rv.error().message());
        return;
    }

    QDir helpDir(tempDirPath + "\\docs");
    if (!QDesktopServices::openUrl(QUrl(helpDir.absolutePath() + "/index.html"))) {
        LOG_CRITICAL("Failed to open help index: {}", rv.error().message());
        return;
    }
}

void MainWindow::about()
{
    QDialog aboutDialog(this);
    aboutDialog.setWindowTitle(tr("About"));
    QVBoxLayout layout(&aboutDialog);
    QLabel logoLabel(&aboutDialog);

    QPixmap logo;
    auto res = Resource::GetResourceData(L"RT_RCDATA", L"LOGO", 0);
    if (res) {
        if (logo.loadFromData(res->data(), res->size(), "PNG")) {
            logoLabel.setPixmap(logo.scaled(100, 100, Qt::KeepAspectRatio));
            layout.addWidget(&logoLabel, 0, Qt::AlignCenter);
        }
    }

    auto text = QString("<h2>DFIR-ORC Forge</h2>"
                        "<p>Version %1</p>"
                        "<p>Configuration editor for DFIR-ORC</p>"
                        "<p>Copyright &copy; 2025 ANSSI</p>")
                    .arg(kDofVersionString);

    QLabel textLabel(&aboutDialog);
    textLabel.setText(text);
    textLabel.setWordWrap(true);
    layout.addWidget(&textLabel, 0, Qt::AlignCenter);

    aboutDialog.setLayout(&layout);
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
