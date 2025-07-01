//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include <QSyntaxHighlighter>

#include <QColor>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QVector>

class XmlHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    XmlHighlighter(QTextDocument *parent = nullptr);

    void updateTheme();

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    struct ColorScheme
    {
        QColor tags;
        QColor attributes;
        QColor values;
        QColor comments;
        QColor declarations;
    };

    void setupThemeColors();
    void setupHighlightingRules();

private:
    QVector<HighlightingRule> m_highlightingRules;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_declarationFormat;
    QTextCharFormat m_tagFormat;
    QTextCharFormat m_attributeFormat;
    QTextCharFormat m_valueFormat;

    ColorScheme m_darkThemeColors;
    ColorScheme m_lightThemeColors;
    ColorScheme m_currentColors;
};
