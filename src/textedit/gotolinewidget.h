//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <QFrame>

#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>

class GotoLineWidget : public QFrame
{
    Q_OBJECT

public:
    explicit GotoLineWidget(QTextEdit *textEdit, QWidget *parent = nullptr);
    void showGotoLine();
    void hideGotoLine();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onLineChanged();
    void gotoLine();

private:
    void setupUI();
    void setupConnections();
    void setupStyling();
    void updatePosition();
    void updateLineInfo();
    int getTotalLines() const;
    int getCurrentLine() const;
    void flashCurrentLine();

private slots:
    void clearFlash();

private:
    QTextEdit *m_textEdit;

    QLineEdit *m_lineEdit;
    QLabel *m_lineInfoLabel;
    QPushButton *m_goButton;
    QPushButton *m_closeButton;

    QTimer m_flashTimer;
    QTextCursor m_originalCursor;
};
