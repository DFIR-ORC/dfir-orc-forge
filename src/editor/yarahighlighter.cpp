//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "yarahighlighter.h"

void setupThemeChangeDetection(YaraHighlighter *highlighter)
{
    class ThemeChangeEventFilter : public QObject
    {
    public:
        explicit ThemeChangeEventFilter(YaraHighlighter *highlighter, QObject *parent = nullptr)
            : QObject(parent)
            , m_highlighter(highlighter)
        {}

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (event->type() == QEvent::ApplicationPaletteChange) {
                m_highlighter->updateTheme();
                //return false;
            }
            return QObject::eventFilter(watched, event);
        }

    private:
        YaraHighlighter *m_highlighter;
    };

    QApplication::instance()->installEventFilter(
        new ThemeChangeEventFilter(highlighter, QApplication::instance()));
}

YaraHighlighter::YaraHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    setupThemeColors();
    setupHighlightingRules();
    setupThemeChangeDetection(this);
}

void YaraHighlighter::updateTheme()
{
    setupThemeColors();
    setupHighlightingRules();
    rehighlight();
}

void YaraHighlighter::highlightBlock(const QString &text)
{
    for (const auto &rule : m_highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);

        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

void YaraHighlighter::setupThemeColors()
{
    bool isDarkTheme = QApplication::palette().color(QPalette::Window).lightness() < 128;

    if (isDarkTheme) {
        m_colors = {
            .keywords = QColor("#569CD6"), // Blue
            .strings = QColor("#CE9178"),  // Light Brown
            .comments = QColor("#6A9955"), // Green
            .numbers = QColor("#B5CEA8")   // Light Green
        };
    } else {
        m_colors = {
            .keywords = QColor("#0000FF"), // Blue
            .strings = QColor("#A31515"),  // Dark Red
            .comments = QColor("#008000"), // Green
            .numbers = QColor("#FF00FF")   // Magenta
        };
    }
}

void YaraHighlighter::setupHighlightingRules()
{
    m_highlightingRules.clear();

    // Keywords format
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(m_colors.keywords);
    keywordFormat.setFontWeight(QFont::Bold);
    m_highlightingRules.push_back(
        {QRegularExpression(R"(\b(rule|meta|strings|condition|and|or|not|true|false)\b)"),
         keywordFormat});

    // Strings format
    QTextCharFormat stringFormat;
    stringFormat.setForeground(m_colors.strings);
    stringFormat.setFontWeight(QFont::Normal);
    m_highlightingRules.push_back({QRegularExpression(R"("[^"]*")"), stringFormat});

    // Comments format
    QTextCharFormat commentFormat;
    commentFormat.setForeground(m_colors.comments);
    commentFormat.setFontItalic(true);
    m_highlightingRules.push_back({QRegularExpression(R"(//[^\n]*)"), commentFormat});

    // Numbers format
    QTextCharFormat numberFormat;
    numberFormat.setForeground(m_colors.numbers);
    numberFormat.setFontWeight(QFont::Normal);
    m_highlightingRules.push_back({QRegularExpression(R"(\b\d+\b)"), numberFormat});
}
