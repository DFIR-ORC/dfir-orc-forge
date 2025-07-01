//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "gotolinewidget.h"

#include <QIntValidator>
#include <QLayout>
#include <QTextBlock>

GotoLineWidget::GotoLineWidget(QTextEdit *textEdit, QWidget *parent)
    : QFrame(parent)
    , m_textEdit(textEdit)
{
    setupUI();
    setupConnections();
    setupStyling();
    hide();
}

void GotoLineWidget::showGotoLine()
{
    updatePosition();
    updateLineInfo();
    show();
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

void GotoLineWidget::hideGotoLine()
{
    hide();
    m_textEdit->setFocus();
}

void GotoLineWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hideGotoLine();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        gotoLine();
        return;
    }

    QFrame::keyPressEvent(event);
}

void GotoLineWidget::onLineChanged()
{
    const auto lineText = m_lineEdit->text();

    if (lineText.isEmpty()) {
        m_goButton->setEnabled(false);
        return;
    }

    bool ok;
    const int lineNumber = lineText.toInt(&ok);

    if (ok && lineNumber >= 1 && lineNumber <= getTotalLines()) {
        m_goButton->setEnabled(true);
    } else {
        m_goButton->setEnabled(false);
    }
}

void GotoLineWidget::gotoLine()
{
    const auto lineText = m_lineEdit->text();
    if (lineText.isEmpty())
        return;

    bool ok;
    const int lineNumber = lineText.toInt(&ok);

    if (!ok || lineNumber < 1)
        return;

    const int totalLines = getTotalLines();
    if (lineNumber > totalLines)
        return;

    // Move cursor to the specified logical line (block)
    // This works correctly with wrapped text as it targets actual line breaks
    auto cursor = m_textEdit->textCursor();

    // Go to the beginning of the document
    cursor.movePosition(QTextCursor::Start);

    // Move to the target block (logical line)
    // blockNumber is 0-based, so we need lineNumber - 1
    auto document = m_textEdit->document();
    auto targetBlock = document->findBlockByNumber(lineNumber - 1);

    if (targetBlock.isValid()) {
        cursor.setPosition(targetBlock.position());
        m_textEdit->setTextCursor(cursor);
        m_textEdit->ensureCursorVisible();

        // Flash the line briefly to show where we went
        //flashCurrentLine();
    }

    hideGotoLine();
}

void GotoLineWidget::setupUI()
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);

    auto *label = new QLabel("Go to line:");

    m_lineEdit = new QLineEdit;
    m_lineEdit->setPlaceholderText("Line number");
    m_lineEdit->setFixedWidth(100);

    // Only allow positive integers
    auto *validator = new QIntValidator(1, 999999, this);
    m_lineEdit->setValidator(validator);

    m_lineInfoLabel = new QLabel;
    m_lineInfoLabel->setMinimumWidth(80);

    m_goButton = new QPushButton("Go");
    m_goButton->setEnabled(false);
    m_goButton->setFixedWidth(50);

    m_closeButton = new QPushButton("✕");
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setToolTip("Close (Esc)");

    layout->addWidget(label);
    layout->addWidget(m_lineEdit);
    layout->addWidget(m_lineInfoLabel);
    layout->addWidget(m_goButton);
    layout->addWidget(m_closeButton);

    // Timer for line flashing effect
    m_flashTimer.setSingleShot(true);
    m_flashTimer.setInterval(200);
}

void GotoLineWidget::setupConnections()
{
    connect(m_lineEdit, &QLineEdit::textChanged, this, &GotoLineWidget::onLineChanged);
    connect(m_goButton, &QPushButton::clicked, this, &GotoLineWidget::gotoLine);
    connect(m_closeButton, &QPushButton::clicked, this, &GotoLineWidget::hideGotoLine);
    //connect(&m_flashTimer, &QTimer::timeout, this, &GotoLineWidget::clearFlash);
}

void GotoLineWidget::setupStyling()
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setLineWidth(1);

    auto font = m_lineInfoLabel->font();
    font.setPointSize(font.pointSize() - 1);
    m_lineInfoLabel->setFont(font);
}

void GotoLineWidget::updatePosition()
{
    if (!m_textEdit || !parentWidget())
        return;

    const auto parentRect = parentWidget()->rect();
    const auto preferredSize = sizeHint();

    // Center horizontally, position near top
    //const int x = (parentRect.width() - preferredSize.width()) / 2;
    //const int y = 50;
    const int x = parentRect.width() - preferredSize.width() - 10;
    const int y = 10;

    setGeometry(x, y, preferredSize.width(), preferredSize.height());
}

void GotoLineWidget::updateLineInfo()
{
    const int totalLines = getTotalLines();
    const int currentLine = getCurrentLine();

    //m_lineInfoLabel->setText(QString("(1-%1, current: %2)").arg(totalLines).arg(currentLine));
    m_lineInfoLabel->setText(QString("(1-%1)").arg(totalLines));
}

int GotoLineWidget::getTotalLines() const
{
    if (!m_textEdit)
        return 0;
    return m_textEdit->document()->blockCount();
}

int GotoLineWidget::getCurrentLine() const
{
    if (!m_textEdit)
        return 1;

    auto cursor = m_textEdit->textCursor();
    return cursor.blockNumber() + 1; // blockNumber is 0-based
}

void GotoLineWidget::flashCurrentLine()
{
    if (!m_textEdit)
        return;

    // Store original selection
    m_originalCursor = m_textEdit->textCursor();

    // Select entire current line
    auto cursor = m_textEdit->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    m_textEdit->setTextCursor(cursor);

    // Start timer to clear selection
    m_flashTimer.start();
}

void GotoLineWidget::clearFlash()
{
    if (!m_textEdit)
        return;

    // Restore original cursor position without selection
    auto cursor = m_originalCursor;
    cursor.clearSelection();
    m_textEdit->setTextCursor(cursor);
}

#include "gotolinewidget.moc"
