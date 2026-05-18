//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <QWidget>

#include <QTextEdit>
#include <QWheelEvent>

#include "gotolinewidget.h"
#include "searchwidget.h"
#include "statuslabel.h"

class TextEditWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TextEditWidget(QWidget *parent = nullptr);
    ~TextEditWidget();

    QTextEdit &textEdit();
    void setWordWrap(bool enable);
    bool wordWrap() const;

protected:
    void setupUI();
    void moveCurrentLineUp();
    void moveCurrentLineDown();

    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

protected:
    QTextEdit *editor_;
    SearchWidget *search_;
    GotoLineWidget *goto_;
    QLabel *position_;
    StatusLabel *status_;
};
