//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "TextEditWidget.h"

#include <format>

#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>

TextEditWidget::TextEditWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

TextEditWidget::~TextEditWidget() {}

void TextEditWidget::setupUI()
{
    auto layout = new QVBoxLayout(this);

    editor_ = new QTextEdit(this);

    QFont font("Consolas"); // alternatives: "Cascadia Mono", "Monospace", "Courier New"
    font.setPointSize(13);
    font.setStyleHint(QFont::TypeWriter);
    editor_->setFont(font);

    editor_->setFrameStyle(
        QFrame::NoFrame); // workaround for font overlap on editor's bottom when using 'setDocumentMode(true)' (resourceedition.cpp)
    editor_->viewport()->installEventFilter(this);
    layout->addWidget(editor_);

    status_ = new StatusLabel(this);
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status_->setMinimumHeight(QFontMetrics(status_->font()).height() + 16);
    status_->setFrameStyle(QFrame::StyledPanel);

    position_ = new QLabel(this);
    position_->setMinimumHeight(QFontMetrics(status_->font()).height() + 16);
    position_->setFrameStyle(QFrame::StyledPanel);
    position_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    connect(editor_, &QTextEdit::cursorPositionChanged, position_, [this]() {
        position_->setText(QString("Ln %1, Col %2")
                               .arg(editor_->textCursor().blockNumber() + 1)
                               .arg(editor_->textCursor().columnNumber()));
    });

    auto *bar = new QFrame();
    bar->setLayout(new QHBoxLayout());
    bar->setFrameStyle(QFrame::NoFrame);
    bar->layout()->setContentsMargins(0, 0, 0, 0);
    bar->layout()->setSpacing(0);
    bar->layout()->addWidget(status_);
    bar->layout()->addWidget(position_);
    bar->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(bar);

    // Search dialog and ctrl+F shortcut
    search_ = new SearchWidget(editor_, editor_);
    auto *searchAction = new QAction(this);
    searchAction->setShortcut(QKeySequence::Find);
    connect(searchAction, &QAction::triggered, [this]() { search_->showSearch(); });
    addAction(searchAction);

    // Goto line dialog and ctrl+G shortcut
    goto_ = new GotoLineWidget(editor_, editor_);
    auto *gotoAction = new QAction(this);
    gotoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(gotoAction, &QAction::triggered, [this]() { goto_->showGotoLine(); });
    addAction(gotoAction);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);
}

QTextEdit &TextEditWidget::textEdit()
{
    return *editor_;
}

void TextEditWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() & Qt::AltModifier) {
        if (event->key() == Qt::Key_Up) {
            moveCurrentLineUp();
            return;
        } else if (event->key() == Qt::Key_Down) {
            moveCurrentLineDown();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void TextEditWidget::moveCurrentLineUp()
{
    auto cursor = editor_->textCursor();
    // Can't move the first line up
    if (cursor.blockNumber() == 0) {
        return;
    }
    // Store current cursor position within the line
    const auto positionInBlock = cursor.positionInBlock();

    // Get current and previous blocks
    auto currentBlock = cursor.block();
    auto previousBlock = currentBlock.previous();

    // Store the plain text of both blocks
    QString currentText = currentBlock.text();
    QString previousText = previousBlock.text();

    // Begin edit block for undo/redo
    cursor.beginEditBlock();

    // Set previous block's text to current block's text
    cursor.setPosition(previousBlock.position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(currentText);

    // Set current block's text to previous block's original text
    cursor.setPosition(currentBlock.position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(previousText);

    // Move cursor back to the correct position in the new line
    cursor.setPosition(previousBlock.position());
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(
        QTextCursor::Right,
        QTextCursor::MoveAnchor,
        std::min(positionInBlock, cursor.block().length() - 1));

    // End edit block
    cursor.endEditBlock();
    editor_->setTextCursor(cursor);
}

void TextEditWidget::moveCurrentLineDown()
{
    auto cursor = editor_->textCursor();
    const auto document = editor_->document();
    // Can't move the last line down
    if (cursor.blockNumber() >= document->blockCount() - 1) {
        return;
    }
    // Store current cursor position within the line
    const auto positionInBlock = cursor.positionInBlock();

    // Get current and next blocks
    auto currentBlock = cursor.block();
    auto nextBlock = currentBlock.next();

    // Store the plain text of both blocks
    QString currentText = currentBlock.text();
    QString nextText = nextBlock.text();

    // Begin edit block for undo/redo
    cursor.beginEditBlock();

    // Set next block's text to current block's text
    cursor.setPosition(nextBlock.position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(currentText);

    // Set current block's text to next block's original text
    cursor.setPosition(currentBlock.position());
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(nextText);

    // Move cursor back to the correct position in the new line
    cursor.setPosition(nextBlock.position());
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(
        QTextCursor::Right,
        QTextCursor::MoveAnchor,
        std::min(positionInBlock, cursor.block().length() - 1));

    // End edit block
    cursor.endEditBlock();
    editor_->setTextCursor(cursor);
}

bool TextEditWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == editor_->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            int baseFontSize = editor_->font().pointSize();
            int delta = wheelEvent->angleDelta().y();

            if (delta > 0 && baseFontSize < 48)
                baseFontSize += 2;
            else if (delta < 0 && baseFontSize > 6)
                baseFontSize -= 2;

            QFont f = editor_->font();
            f.setPointSize(baseFontSize);
            editor_->setFont(f);

            wheelEvent->accept();
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}
