//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include <QApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class YaraHighlighter;

void setupThemeChangeDetection(YaraHighlighter *highlighter);

class YaraHighlighter : public QSyntaxHighlighter
{
public:
    explicit YaraHighlighter(QTextDocument *parent = nullptr);

    void updateTheme();

protected:
    void highlightBlock(const QString &text) override;
    void setupThemeColors();
    void setupHighlightingRules();

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    struct ThemeColors
    {
        QColor keywords;
        QColor strings;
        QColor comments;
        QColor numbers;
    };

    ThemeColors m_colors;
    std::vector<HighlightingRule> m_highlightingRules;
};
