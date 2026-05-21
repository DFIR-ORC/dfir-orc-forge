//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "xmlhighlighter.h"

#include <QApplication>
#include <QPalette>

void setupThemeChangeDetection(XmlHighlighter *highlighter)
{
    class ThemeChangeEventFilter : public QObject
    {
    public:
        explicit ThemeChangeEventFilter(XmlHighlighter *highlighter, QObject *parent = nullptr)
            : QObject(parent)
            , m_highlighter(highlighter)
        {}

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (event->type() == QEvent::ApplicationPaletteChange) {
                m_highlighter->updateTheme();
                return false;
            }
            return QObject::eventFilter(watched, event);
        }

    private:
        XmlHighlighter *m_highlighter;
    };

    QApplication::instance()->installEventFilter(
        new ThemeChangeEventFilter(highlighter, highlighter));
}

XmlHighlighter::XmlHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    m_darkThemeColors
        = {.tags = QColor("#61AFEF"),
           .attributes = QColor("#E06C75"),
           .values = QColor("#98C379"),
           .comments = QColor("#7F848E"),
           .declarations = QColor("#C678DD")};

    m_lightThemeColors
        = {.tags = QColor("#0000FF"),
           .attributes = QColor("#FF4500"),
           .values = QColor("#008000"),
           .comments = QColor("#808080"),
           .declarations = QColor("#800080")};

    setupThemeChangeDetection(this);
    updateTheme();
}

void XmlHighlighter::updateTheme()
{
    setupThemeColors();
    setupHighlightingRules();
    rehighlight();
}

void XmlHighlighter::setupThemeColors()
{
    bool isDarkTheme = QApplication::palette().color(QPalette::Window).lightness() < 128;
    m_currentColors = isDarkTheme ? m_darkThemeColors : m_lightThemeColors;
}

void XmlHighlighter::setupHighlightingRules()
{
    m_highlightingRules.clear();

    // XML element format
    m_tagFormat.setForeground(m_currentColors.tags);
    m_tagFormat.setFontWeight(QFont::Bold);
    m_highlightingRules.append(
        {QRegularExpression(R"((<[!?]?[a-zA-Z0-9-_]+)|(/?>)|(</[a-zA-Z0-9-_]+>))"), m_tagFormat});

    // XML attribute format
    m_attributeFormat.setForeground(m_currentColors.attributes);
    m_attributeFormat.setFontWeight(QFont::Normal);
    m_highlightingRules.append({QRegularExpression(R"(\w+(?=\=))"), m_attributeFormat});

    // XML value format
    m_valueFormat.setForeground(m_currentColors.values);
    m_valueFormat.setFontWeight(QFont::Normal);
    m_highlightingRules.append({QRegularExpression(R"("[^"]*"|'[^']*')"), m_valueFormat});

    // XML comment format
    m_commentFormat.setForeground(m_currentColors.comments);
    m_commentFormat.setFontWeight(QFont::Normal);

    // XML declaration format
    m_declarationFormat.setForeground(m_currentColors.declarations);
    m_declarationFormat.setFontWeight(QFont::Normal);
    m_highlightingRules.append({QRegularExpression(R"(<\?xml.*\?>)"), m_declarationFormat});
}

void XmlHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : m_highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Handle multiline comments
    static const QRegularExpression commentStart(R"(<!--)");
    static const QRegularExpression commentEnd(R"(-->)");

    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() == 1) {
        startIndex = 0;
    } else {
        startIndex = text.indexOf(commentStart);
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch endMatch = commentEnd.match(text, startIndex);
        int endIndex = endMatch.capturedStart();
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, m_commentFormat);
        startIndex = text.indexOf(commentStart, startIndex + commentLength);
    }
}
