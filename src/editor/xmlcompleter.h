//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <QCompleter>
#include <QModelIndex>
#include <QObject>
#include <QStringList>
#include <QTextEdit>

class XsdStore;

class XmlCompleter : public QObject
{
    Q_OBJECT

public:
    explicit XmlCompleter(XsdStore &xsdStore, QObject *parent = nullptr);

    void attach(QTextEdit *textEdit);

public slots:
    void setConfigIdentifier(const QString &id);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    struct CompletionContext
    {
        enum class Type { None, Element, Attribute, Value, TextContent };

        Type type = Type::None;
        QString prefix;
        QString elementName;
        QString parentElementName; // enclosing element (Element type)
        QString attributeName;     // for Value type
        bool isClosingTag = false; // for Element type only
    };

    // Key event handling
    void triggerCompletion();
    void acceptCompletion();
    void insertCompletion(const QString &completion);
    void onPopupCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

    // Context parsing
    CompletionContext analyzeContext(const QString &textUpToCursor) const;

    // XSD querying
    QStringList getElementNames(const QString &parentElement) const;
    QStringList getAttributeNames(const QString &elementName) const;
    QStringList getAttributeValues(const QString &elementName, const QString &attributeName) const;
    QStringList getElementContentValues(const QString &elementName) const;

private:
    XsdStore &xsdStore_;
    QTextEdit *textEdit_ = nullptr;
    QCompleter *completer_ = nullptr;
    QString configIdentifier_;
    CompletionContext currentCtx_;
    bool inserting_ = false;
    bool suppressNextTrigger_ = false;
};
