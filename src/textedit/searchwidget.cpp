//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "searchwidget.h"

#include <algorithm>
#include <ranges>
#include <vector>

#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QScrollbar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>

SearchWidget::SearchWidget(QTextEdit *textEdit, QWidget *parent)
    : QFrame(parent)
    , m_textEdit(textEdit)
    , m_currentMatchIndex(-1)
{
    setupUI();
    setupConnections();
    setupStyling();
    // Force native window creation and style polishing while the parent is still
    // hidden, so the first real show() is just a repaint with no stutter.
    show();
    hide();
}

void SearchWidget::showSearch()
{
    updatePosition();
    show();
    m_searchLineEdit->setFocus();
    m_searchLineEdit->selectAll();
}

void SearchWidget::hideSearch()
{
    hide();
    clearHighlights();
    m_textEdit->setFocus();
}

void SearchWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hideSearch();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() & Qt::ShiftModifier) {
            findPrevious();
        } else {
            findNext();
        }
        return;
    }

    QFrame::keyPressEvent(event);
}

void SearchWidget::onTextChanged()
{
    const auto searchText = m_searchLineEdit->text();

    if (searchText.isEmpty()) {
        clearHighlights();
        updateMatchInfo();
        return;
    }

    m_searchTimer.stop();
    m_searchTimer.start(150);
}

void SearchWidget::performSearch()
{
    const auto searchText = m_searchLineEdit->text();
    if (searchText.isEmpty()) {
        clearHighlights();
        updateMatchInfo();
        return;
    }

    findAllMatches(searchText);
    highlightMatches();
    updateMatchInfo();

    if (!m_matches.empty()) {
        m_currentMatchIndex = 0;
        highlightCurrentMatch();
    }
}

void SearchWidget::findNext()
{
    if (m_matches.empty())
        return;

    m_currentMatchIndex = (m_currentMatchIndex + 1) % static_cast<int>(m_matches.size());
    highlightCurrentMatch();
    updateMatchInfo();
}

void SearchWidget::findPrevious()
{
    if (m_matches.empty())
        return;

    m_currentMatchIndex = (m_currentMatchIndex - 1 + static_cast<int>(m_matches.size()))
                          % static_cast<int>(m_matches.size());
    highlightCurrentMatch();
    updateMatchInfo();
}

void SearchWidget::setupUI()
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    m_searchLineEdit = new QLineEdit;
    m_searchLineEdit->setPlaceholderText("Find...");
    m_searchLineEdit->setMinimumWidth(200);

    m_findPrevButton = new QPushButton("↑");
    m_findPrevButton->setFixedSize(24, 24);
    m_findPrevButton->setToolTip("Previous (Shift+Enter)");

    m_findNextButton = new QPushButton("↓");
    m_findNextButton->setFixedSize(24, 24);
    m_findNextButton->setToolTip("Next (Enter)");

    m_matchLabel = new QLabel("0 of 0");
    m_matchLabel->setMinimumWidth(m_matchLabel->fontMetrics().horizontalAdvance("100 of 100") + 8);
    m_matchLabel->setAlignment(Qt::AlignCenter);

    m_closeButton = new QPushButton("✕");
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setToolTip("Close (Esc)");

    layout->addWidget(m_searchLineEdit);
    layout->addWidget(m_findPrevButton);
    layout->addWidget(m_findNextButton);
    layout->addWidget(m_matchLabel);
    layout->addWidget(m_closeButton);

    m_searchTimer.setSingleShot(true);
}

void SearchWidget::setupConnections()
{
    connect(m_searchLineEdit, &QLineEdit::textChanged, this, &SearchWidget::onTextChanged);
    connect(&m_searchTimer, &QTimer::timeout, this, &SearchWidget::performSearch);
    connect(m_findNextButton, &QPushButton::clicked, this, &SearchWidget::findNext);
    connect(m_findPrevButton, &QPushButton::clicked, this, &SearchWidget::findPrevious);
    connect(m_closeButton, &QPushButton::clicked, this, &SearchWidget::hideSearch);
}

void SearchWidget::setupStyling()
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setLineWidth(1);

    auto font = m_matchLabel->font();
    font.setPointSize(font.pointSize() - 1);
    m_matchLabel->setFont(font);
}

void SearchWidget::updatePosition()
{
    if (!m_textEdit || !parentWidget())
        return;

    const auto parentRect = parentWidget()->rect();
    const auto preferredSize = sizeHint();

    const int x = parentRect.width() - preferredSize.width() - 10;
    const int y = 10;

    setGeometry(x, y, preferredSize.width(), preferredSize.height());
}

void SearchWidget::findAllMatches(const QString &searchText)
{
    m_matches.clear();

    if (searchText.isEmpty())
        return;

    const auto document = m_textEdit->document();

    auto cursor = document->find(searchText, 0);
    while (!cursor.isNull()) {
        m_matches.emplace_back(cursor.selectionStart(), searchText.length());
        cursor = document->find(searchText, cursor);
    }

    std::ranges::sort(m_matches, [](const Match &a, const Match &b) {
        return a.position < b.position;
    });
}

void SearchWidget::highlightMatches()
{
    clearHighlights();

    if (m_matches.empty())
        return;

    auto cursor = m_textEdit->textCursor();

    QTextCharFormat highlightFormat;
    highlightFormat.setBackground(QColor(255, 255, 0, 120));

    for (const auto &match : m_matches) {
        cursor.setPosition(match.position);
        cursor.setPosition(match.position + match.length, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(highlightFormat);
    }
}

void SearchWidget::highlightCurrentMatch()
{
    if (m_currentMatchIndex < 0 || m_currentMatchIndex >= static_cast<int>(m_matches.size())) {
        return;
    }

    const auto &match = m_matches[m_currentMatchIndex];

    auto cursor = m_textEdit->textCursor();
    cursor.setPosition(match.position);
    cursor.setPosition(match.position + match.length, QTextCursor::KeepAnchor);

    QTextCharFormat currentFormat;
    currentFormat.setBackground(QColor(255, 140, 0, 180));
    cursor.mergeCharFormat(currentFormat);

    m_textEdit->setTextCursor(cursor);
    m_textEdit->ensureCursorVisible();
}

void SearchWidget::clearHighlights()
{
    if (!m_textEdit)
        return;

    auto originalCursor = m_textEdit->textCursor();
    auto originalPosition = originalCursor.position();
    auto originalScrollPosition = m_textEdit->verticalScrollBar()->value();

    auto cursor = m_textEdit->textCursor();
    cursor.select(QTextCursor::Document);

    auto background
        = m_textEdit->palette().color(QPalette::ColorGroup::Normal, QPalette::ColorRole::Window);

    QTextCharFormat clearFormat;
    clearFormat.setBackground(background);
    cursor.mergeCharFormat(clearFormat);

    cursor.clearSelection();
    m_textEdit->setTextCursor(cursor);

    cursor.setPosition(originalPosition);
    m_textEdit->setTextCursor(cursor);
    m_textEdit->verticalScrollBar()->setValue(originalScrollPosition);
}

void SearchWidget::updateMatchInfo()
{
    if (m_matches.empty()) {
        m_matchLabel->setText("0 of 0");
        m_findPrevButton->setEnabled(false);
        m_findNextButton->setEnabled(false);
    } else {
        const auto currentMatch = m_currentMatchIndex >= 0 ? m_currentMatchIndex + 1 : 0;
        m_matchLabel->setText(QString("%1 of %2").arg(currentMatch).arg(m_matches.size()));
        m_findPrevButton->setEnabled(true);
        m_findNextButton->setEnabled(true);
    }
}

#include "searchwidget.moc"
