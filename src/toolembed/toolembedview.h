//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <QString>
#include <QWidget>

class ProcessOutputView;
class QPushButton;
class QStackedWidget;
class ToolEmbedSetupView;

// DFIR-ORC-specific wrapper around ProcessOutputView.
// Phase 0 - ToolEmbedSetupView: lets the user review / override the
//   executable, configuration folder and pending downloads, then click Create.
// Phase 1 - ProcessOutputView: streams toolembed stdout/stderr live.
// After a successful run a "Save and open" button becomes available.
// Emits fileReady(savedPath) once the file has been saved.
class ToolEmbedView : public QWidget
{
    Q_OBJECT

public:
    explicit ToolEmbedView(QWidget *parent = nullptr);

    // Shows the setup panel (resets to the setup page for a new run).
    void promptAndRun();

signals:
    void fileReady(QString savedPath);

private:
    void run(const QString &exe, const QString &embedConfig, const QString &configFolder);

    QStackedWidget *stack_;
    ToolEmbedSetupView *setupView_;
    ProcessOutputView *outputView_;
    QPushButton *saveButton_;
    int runCounter_ = 0;
};
