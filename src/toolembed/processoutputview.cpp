//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "processoutputview.h"

#include <QColor>
#include <QFont>
#include <QLabel>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>

#include "processoutputview.moc"

#include "utils/log.h"

namespace {

// Returns the text color for a line based on its log-level tag
// ([D] debug, [I] info, [W] warning, [E] error, [C] critical).
QColor colorForLine(const QString &line)
{
    if (line.contains(QLatin1String("[C]")))
        return QColor(0xff, 0x55, 0xff); // magenta  - critical
    if (line.contains(QLatin1String("[E]")))
        return QColor(0xff, 0x55, 0x55); // red       - error
    if (line.contains(QLatin1String("[W]")))
        return QColor(0xff, 0xc0, 0x00); // amber     - warning
    if (line.contains(QLatin1String("[D]")))
        return QColor(0x88, 0x88, 0x88); // dark gray - debug
    return QColor(0xcc, 0xcc, 0xcc);     // light gray - info / unrecognised
}

} // namespace

ProcessOutputView::ProcessOutputView(QWidget *parent)
    : QWidget(parent)
    , status_(new QLabel(this))
    , output_(new QTextEdit(this))
    , process_(new QProcess(this))
    , runningMessage_("Running...")
    , successMessage_("Process completed successfully.")
    , failureMessage_("Process failed.")
{
    QFont mono("Consolas", 9);
    mono.setStyleHint(QFont::Monospace);

    output_->setReadOnly(true);
    output_->setFont(mono);
    output_->setLineWrapMode(QTextEdit::NoWrap);

    QPalette pal = output_->palette();
    pal.setColor(QPalette::Base, QColor(0x0c, 0x0c, 0x0c));
    pal.setColor(QPalette::Text, QColor(0xcc, 0xcc, 0xcc));
    output_->setPalette(pal);

    QFont bold = status_->font();
    bold.setBold(true);
    status_->setFont(bold);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(status_);
    layout->addWidget(output_);
    setLayout(layout);

    connect(
        process_,
        &QProcess::readyReadStandardOutput,
        this,
        &ProcessOutputView::onReadyReadStandardOutput);
    connect(
        process_,
        &QProcess::readyReadStandardError,
        this,
        &ProcessOutputView::onReadyReadStandardError);
    connect(process_, &QProcess::finished, this, &ProcessOutputView::onFinished);
}

ProcessOutputView::~ProcessOutputView()
{
    if (process_->state() != QProcess::NotRunning) {
        process_->disconnect(this);
        process_->terminate();
        if (!process_->waitForFinished(3000))
            process_->kill();
        process_->waitForFinished(1000);
    }
}

void ProcessOutputView::run(
    const QString &program, const QStringList &arguments, const QString &outPath)
{
    outPath_ = outPath;
    output_->clear();
    lineBuffer_.clear();
    status_->setText(runningMessage_);

    process_->start(program, arguments);

    if (!process_->waitForStarted(5000)) {
        const auto errorStr = process_->errorString().toStdString();
        LOG_CRITICAL("Failed to start process: {}", errorStr);
        appendLine(QString("Failed to start process: %1").arg(process_->errorString()));
        status_->setText(failureMessage_);
        emit processFailed();
    }
}

void ProcessOutputView::onReadyReadStandardOutput()
{
    appendOutput(QString::fromLocal8Bit(process_->readAllStandardOutput()));
}

void ProcessOutputView::onReadyReadStandardError()
{
    appendOutput(QString::fromLocal8Bit(process_->readAllStandardError()));
}

void ProcessOutputView::onFinished(int exitCode, QProcess::ExitStatus /*exitStatus*/)
{
    appendOutput(QString::fromLocal8Bit(process_->readAllStandardOutput()));
    appendOutput(QString::fromLocal8Bit(process_->readAllStandardError()));
    flushLineBuffer();

    if (exitCode != 0) {
        LOG_CRITICAL("Process exited with code {}", exitCode);
        status_->setText(QString("%1 (exit code %2)").arg(failureMessage_).arg(exitCode));
        emit processFailed();
        return;
    }

    status_->setText(successMessage_);
    emit processSucceeded(outPath_);
}

void ProcessOutputView::setRunningMessage(const QString &msg)
{
    runningMessage_ = msg;
}
void ProcessOutputView::setSuccessMessage(const QString &msg)
{
    successMessage_ = msg;
}
void ProcessOutputView::setFailureMessage(const QString &msg)
{
    failureMessage_ = msg;
}

void ProcessOutputView::appendOutput(const QString &text)
{
    if (text.isEmpty())
        return;

    QString normalized = text;
    normalized.remove('\r');

    lineBuffer_ += normalized;

    int newline = 0;
    while ((newline = lineBuffer_.indexOf('\n')) != -1) {
        appendLine(lineBuffer_.left(newline));
        lineBuffer_ = lineBuffer_.mid(newline + 1);
    }
}

void ProcessOutputView::flushLineBuffer()
{
    if (!lineBuffer_.isEmpty()) {
        appendLine(lineBuffer_);
        lineBuffer_.clear();
    }
}

void ProcessOutputView::appendLine(const QString &line)
{
    QTextCharFormat fmt;
    fmt.setForeground(colorForLine(line));

    QTextCursor cursor = output_->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!output_->document()->isEmpty())
        cursor.insertText("\n", fmt);
    cursor.insertText(line, fmt);
    output_->setTextCursor(cursor);
}
