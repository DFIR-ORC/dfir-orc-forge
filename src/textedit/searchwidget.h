//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <QFrame>

#include <vector>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>

class SearchWidget : public QFrame
{
    Q_OBJECT

public:
    explicit SearchWidget(QTextEdit *textEdit, QWidget *parent = nullptr);

    void showSearch();
    void hideSearch();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onTextChanged();
    void performSearch();
    void findNext();
    void findPrevious();

private:
    struct Match
    {
        int position;
        int length;
    };

    void setupUI();
    void setupConnections();
    void setupStyling();
    void updatePosition();
    void findAllMatches(const QString &searchText);
    void highlightMatches();
    void highlightCurrentMatch();
    void clearHighlights();
    void updateMatchInfo();

private:
    QTextEdit *m_textEdit;

    QLineEdit *m_searchLineEdit;
    QPushButton *m_findPrevButton;
    QPushButton *m_findNextButton;
    QPushButton *m_closeButton;
    QLabel *m_matchLabel;

    std::vector<Match> m_matches;
    int m_currentMatchIndex;
    QTimer m_searchTimer;
};
