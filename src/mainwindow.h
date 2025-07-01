//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <QMainWindow>

#include <QAction>
#include <QMenu>
#include <QMenuBar>

#include "editor/editorinterface.h"

class XsdStore;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void loadFile(QString path);

public slots:
    void newFile(QString content, EditorFormat format);
    void openFileDialog();
    void saveFile();
    void saveFileAs();
    void help();
    void about();

private:
    void closeEvent(QCloseEvent *event) override;
    void setupUI();
    bool hasModifiedStatus() const;
    void setModifiedStatus(bool value);

private:
    std::optional<QString> path_;
    QString windowTitle_;
    std::unique_ptr<XsdStore> xsdStore_;

    QMenuBar *menuBar_;
    QMenu *fileMenu_;
    QMenu *xmlConfigurationMenu_;
    QMenu *helpMenu_;
    QAction *actionOpen_;
    QAction *actionSave_;
    QAction *actionSaveAs_;
    QAction *actionExit_;
    QAction *actionViewHelp_;
    QAction *actionViewOnlineHelp_;
    QAction *actionAbout_;
    QAction *actionReloadXsd_;
    QAction *actionNewXmlFastFind_;
    QAction *actionNewXmlFatInfo_;
    QAction *actionNewXmlGetSamples_;
    QAction *actionNewXmlGetSectors_;
    QAction *actionNewXmlGetThis_;
    QAction *actionNewXmlLocal_;
    QAction *actionNewXmlNtfsInfo_;
    QAction *actionNewXmlObjInfo_;
    QAction *actionNewXmlRegInfo_;
    QAction *actionNewXmlUsnInfo_;
    QAction *actionNewXmlWolf_;
    QAction *actionNewYaraRule_;
};
