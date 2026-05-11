//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTextEdit>
#include <QWidget>

class QLabel;

// Generic widget that runs a child process, streams its stdout/stderr live
// into a read-only QTextEdit and colorizes lines by log-level tag
// ([D] / [I] / [W] / [E] / [C]).
// Emits processSucceeded(outPath) or processFailed() when the process exits.
class ProcessOutputView : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessOutputView(QWidget *parent = nullptr);
    ~ProcessOutputView() override;

    void run(const QString &program, const QStringList &arguments, const QString &outPath);

    void setRunningMessage(const QString &msg);
    void setSuccessMessage(const QString &msg);
    void setFailureMessage(const QString &msg);

signals:
    void processSucceeded(QString outPath);
    void processFailed();

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void appendOutput(const QString &text);
    void flushLineBuffer();
    void appendLine(const QString &line);

    QLabel *status_;
    QTextEdit *output_;
    QProcess *process_;
    QString outPath_;
    QString lineBuffer_;
    QString runningMessage_;
    QString successMessage_;
    QString failureMessage_;
};
