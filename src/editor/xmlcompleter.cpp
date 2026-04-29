//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "xmlcompleter.h"

#import "msxml6.dll" rename_namespace("MSXML")

#include <comdef.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QKeyEvent>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextCursor>
#include <QToolTip>

#include "utils/guard.h"
#include "xsdstore.h"

// =============================================================================
// Internal XSD-querying helpers (anonymous namespace, internal linkage)
// =============================================================================
namespace {

using DocList = std::vector<MSXML::IXMLDOMDocument3Ptr>;

// -----------------------------------------------------------------------------
// MSXML primitives
// -----------------------------------------------------------------------------

// Read a named XML attribute from a DOM node. Returns empty string on failure.
QString nodeAttributeValue(MSXML::IXMLDOMNodePtr node, const wchar_t *attrName)
{
    IXMLDOMElementPtr elem;
    if (FAILED(node->QueryInterface(IID_IXMLDOMElement, reinterpret_cast<void **>(&elem))) || !elem)
        return {};
    variant_t val;
    if (FAILED(elem->getAttribute(bstr_t(attrName), &val)) || val.vt != VT_BSTR)
        return {};
    return QString::fromWCharArray(val.bstrVal);
}

// Run xpath on every document; collect attrName values from all matching nodes.
QStringList selectAttributeValues(const DocList &docs, const QString &xpath, const wchar_t *attrName)
{
    QStringList values;
    const bstr_t xpathBstr(xpath.toStdWString().c_str());
    for (const auto &doc : docs) {
        if (!doc)
            continue;
        MSXML::IXMLDOMNodeListPtr nl;
        if (FAILED(doc->raw_selectNodes(xpathBstr, &nl)) || !nl)
            continue;
        long len = 0;
        nl->get_length(&len);
        for (long i = 0; i < len; ++i) {
            MSXML::IXMLDOMNodePtr node;
            if (FAILED(nl->get_item(i, &node)) || !node)
                continue;
            const QString v = nodeAttributeValue(node, attrName);
            if (!v.isEmpty())
                values << v;
        }
    }
    values.removeDuplicates();
    return values;
}

// Return true if xpath matches at least one node in any document.
bool xpathExists(const DocList &docs, const QString &xpath)
{
    const bstr_t xpathBstr(xpath.toStdWString().c_str());
    for (const auto &doc : docs) {
        if (!doc)
            continue;
        MSXML::IXMLDOMNodeListPtr nl;
        if (FAILED(doc->raw_selectNodes(xpathBstr, &nl)) || !nl)
            continue;
        long len = 0;
        nl->get_length(&len);
        if (len > 0)
            return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Convenience wrappers
// -----------------------------------------------------------------------------

// Collect @name values matching xpath across all documents.
QStringList selectNames(const DocList &docs, const QString &xpath)
{
    return selectAttributeValues(docs, xpath, L"name");
}

// Collect xs:enumeration/@value items for a named simpleType.
QStringList getEnumerationValues(const QString &typeName, const DocList &docs)
{
    return selectAttributeValues(
        docs,
        QString("//xs:simpleType[@name='%1']//xs:enumeration[@value]").arg(typeName),
        L"value");
}

// Build the two-document list (per-config XSD + common.xsd) used by all queries.
DocList buildDocList(const XsdStore &store, const QString &id)
{
    DocList docs;
    if (auto r = store.get(id))
        docs.push_back(*r);
    if (auto c = store.getCommonDocument())
        docs.push_back(c);
    return docs;
}

// XPath predicate for case-insensitive @name match against a user-supplied name.
// Used wherever names come from XML content (potentially lowercase) but the XSD
// may use PascalCase. XSD-internal references (type names, group names) are
// already consistent in case and do not need this.
QString ciName(const QString &name)
{
    return QString(
               "[translate(@name,'ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz')='%1']")
        .arg(name.toLower());
}

// -----------------------------------------------------------------------------
// Type resolution
// -----------------------------------------------------------------------------

// Return the @type of the first xs:element[@name] found, or empty.
QString resolveElementTypeName(const QString &elementName, const DocList &docs)
{
    const auto types
        = selectAttributeValues(docs, QString("//xs:element%1").arg(ciName(elementName)), L"type");
    return types.isEmpty() ? QString() : types.first();
}

// Return the XPath base for an element's complexType (inline or named).
QString elementTypeBaseXp(const QString &elementName, const DocList &docs)
{
    const QString t = resolveElementTypeName(elementName, docs);
    return t.isEmpty() ? QString("//xs:element%1//xs:complexType").arg(ciName(elementName))
                       : QString("//xs:complexType[@name='%1']").arg(t);
}

// -----------------------------------------------------------------------------
// Attribute name completion
// -----------------------------------------------------------------------------

// Collect xs:attribute/@name under baseXp, following xs:attributeGroup refs.
QStringList resolveAttributesFromBase(const QString &baseXp, const DocList &docs)
{
    QStringList names = selectNames(docs, baseXp + "//xs:attribute[@name]");

    const auto groupRefs = selectAttributeValues(docs, baseXp + "//xs:attributeGroup[@ref]", L"ref");
    for (const QString &g : groupRefs)
        names << selectNames(
            docs, QString("//xs:attributeGroup[@name='%1']//xs:attribute[@name]").arg(g));

    names.removeDuplicates();
    return names;
}

// -----------------------------------------------------------------------------
// Attribute value completion
// -----------------------------------------------------------------------------

// Find @type of xs:attribute[@name=attrName] under baseXp, following
// xs:attributeGroup refs recursively.
QString findAttributeTypeFromBase(
    const QString &baseXp, const QString &attributeName, const DocList &docs)
{
    const auto types = selectAttributeValues(
        docs, baseXp + QString("//xs:attribute%1").arg(ciName(attributeName)), L"type");
    if (!types.isEmpty())
        return types.first();

    const auto groupRefs = selectAttributeValues(docs, baseXp + "//xs:attributeGroup[@ref]", L"ref");
    for (const QString &g : groupRefs) {
        const QString t = findAttributeTypeFromBase(
            QString("//xs:attributeGroup[@name='%1']").arg(g), attributeName, docs);
        if (!t.isEmpty())
            return t;
    }
    return {};
}

// -----------------------------------------------------------------------------
// Element self-closing check
// -----------------------------------------------------------------------------

// True when the element's XSD type has no child xs:element nodes and no
// xs:simpleContent (i.e. it should be written as <element/>).
bool isElementSelfClosing(const QString &elementName, const DocList &docs)
{
    const QString base = elementTypeBaseXp(elementName, docs);
    return !xpathExists(docs, base + "//xs:element")
           && !xpathExists(docs, base + "/xs:simpleContent");
}

// -----------------------------------------------------------------------------
// Text content completion
// -----------------------------------------------------------------------------

// Resolve an element type name to its actual simple-content base type:
//   xs:complexType with xs:simpleContent -> xs:extension/@base
//   xs:simpleType used directly          -> the type name itself
QString resolveElementContentTypeName(const QString &typeName, const DocList &docs)
{
    const auto bases = selectAttributeValues(
        docs,
        QString("//xs:complexType[@name='%1']/xs:simpleContent/xs:extension").arg(typeName),
        L"base");
    if (!bases.isEmpty())
        return bases.first();

    if (xpathExists(docs, QString("//xs:simpleType[@name='%1']").arg(typeName)))
        return typeName;

    return {};
}

// Return completable tokens for a simpleType: xs:enumeration values, or (as a
// fallback) the first alternation group of an xs:pattern: "(val1|val2|...)(,...)*".
QStringList getValuesFromSimpleType(const QString &typeName, const DocList &docs)
{
    QStringList values = getEnumerationValues(typeName, docs);
    if (!values.isEmpty())
        return values;

    const auto patterns = selectAttributeValues(
        docs, QString("//xs:simpleType[@name='%1']//xs:pattern").arg(typeName), L"value");

    for (const QString &pattern : patterns) {
        const int start = pattern.indexOf('(');
        const int end = (start >= 0) ? pattern.indexOf(')', start) : -1;
        if (end < 0)
            continue;
        for (const QString &token : pattern.mid(start + 1, end - start - 1).split('|')) {
            const QString t = token.trimmed();
            bool plain = !t.isEmpty();
            for (const QChar c : t) {
                if (!c.isLetterOrNumber() && c != '_' && c != '-') {
                    plain = false;
                    break;
                }
            }
            if (plain)
                values << t;
        }
        if (!values.isEmpty())
            break;
    }

    values.removeDuplicates();
    return values;
}

// -----------------------------------------------------------------------------
// Documentation extraction
// -----------------------------------------------------------------------------

// Return the trimmed text content of the first node matching xpath, or empty.
QString selectNodeText(const DocList &docs, const QString &xpath)
{
    const bstr_t xpathBstr(xpath.toStdWString().c_str());
    for (const auto &doc : docs) {
        if (!doc)
            continue;
        MSXML::IXMLDOMNodePtr node;
        if (FAILED(doc->raw_selectSingleNode(xpathBstr, &node)) || !node)
            continue;
        Guard::Bstr raw;
        if (FAILED(node->get_text(raw.addressOf())) || !raw)
            continue;
        const QString raw_text = QString::fromWCharArray(raw.get());

        // Normalize per-line: collapse intra-line whitespace but preserve
        // intentional line breaks (real newlines or &#10; entities in XSD).
        QStringList lines = raw_text.split('\n');
        for (QString &line : lines)
            line = line.simplified(); // trim + collapse spaces within each line
        // Strip leading/trailing blank lines introduced by XML indentation.
        while (!lines.isEmpty() && lines.first().isEmpty())
            lines.removeFirst();
        while (!lines.isEmpty() && lines.last().isEmpty())
            lines.removeLast();

        const QString text = lines.join('\n');
        if (!text.isEmpty())
            return text;
    }
    return {};
}

// Return the xs:annotation/xs:documentation text for a named xs:element.
QString getElementDocumentation(const QString &elementName, const DocList &docs)
{
    return selectNodeText(
        docs,
        QString("//xs:element[@name='%1'][1]/xs:annotation/xs:documentation").arg(elementName));
}

// Return the xs:annotation/xs:documentation text for a named xs:attribute,
// searched first under the element's complexType base, then globally.
QString getAttributeDocumentation(
    const QString &elementName, const QString &attributeName, const DocList &docs)
{
    const QString base = elementTypeBaseXp(elementName, docs);

    // Try element-scoped definition first (direct or via attributeGroup).
    for (const QString &xp : {
             base
                 + QString("//xs:attribute[@name='%1'][1]/xs:annotation/xs:documentation")
                       .arg(attributeName),
         }) {
        const QString t = selectNodeText(docs, xp);
        if (!t.isEmpty())
            return t;
    }

    // Fall back to any global xs:attribute definition.
    return selectNodeText(
        docs,
        QString("//xs:attribute[@name='%1'][1]/xs:annotation/xs:documentation").arg(attributeName));
}

// -----------------------------------------------------------------------------
// Parent element detection
// -----------------------------------------------------------------------------

// Walk the XML text forward, maintaining an open-tag stack, and return the
// name of the innermost unclosed element (i.e. the parent of the cursor).
QString findParentElement(const QString &text)
{
    QStringList stack;
    int i = 0;
    while (i < text.size()) {
        if (text[i] != '<') {
            ++i;
            continue;
        }
        ++i;
        if (i >= text.size())
            break;

        // Skip processing instructions (<?...) and comments (<!--...).
        if (text[i] == '?' || text[i] == '!') {
            while (i < text.size() && text[i] != '>')
                ++i;
            ++i;
            continue;
        }

        const bool isClose = (text[i] == '/');
        if (isClose)
            ++i;

        // Read tag name (stop at whitespace, '>', '/').
        const int nameStart = i;
        while (i < text.size() && !text[i].isSpace() && text[i] != '>' && text[i] != '/')
            ++i;
        const QString tagName = text.mid(nameStart, i - nameStart);
        if (tagName.isEmpty()) {
            ++i;
            continue;
        }

        // Scan to end of tag; note self-closing ('/').
        bool selfClose = false;
        while (i < text.size() && text[i] != '>') {
            if (text[i] == '/')
                selfClose = true;
            ++i;
        }
        ++i; // skip '>'

        if (selfClose || isClose) {
            if (!isClose)
                continue; // self-closing: no stack change
            // Closing tag: pop matching entry (handle mismatches gracefully).
            for (int k = stack.size() - 1; k >= 0; --k) {
                if (stack[k].compare(tagName, Qt::CaseInsensitive) == 0) {
                    stack.remove(k);
                    break;
                }
            }
        } else {
            stack.append(tagName);
        }
    }
    return stack.isEmpty() ? QString() : stack.last();
}

} // namespace

// =============================================================================
// XmlCompleter
// =============================================================================

XmlCompleter::XmlCompleter(XsdStore &xsdStore, QObject *parent)
    : QObject(parent)
    , xsdStore_(xsdStore)
{
    completer_ = new QCompleter(this);
    completer_->setModel(new QStringListModel(completer_));
    completer_->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setWrapAround(false);
    completer_->setCompletionMode(QCompleter::PopupCompletion);

    connect(
        completer_,
        QOverload<const QString &>::of(&QCompleter::activated),
        this,
        &XmlCompleter::insertCompletion);

    connect(
        completer_->popup()->selectionModel(),
        &QItemSelectionModel::currentChanged,
        this,
        &XmlCompleter::onPopupCurrentChanged);
}

void XmlCompleter::attach(QTextEdit *textEdit)
{
    textEdit_ = textEdit;
    textEdit_->installEventFilter(this);
    completer_->setWidget(textEdit_);
    completer_->popup()->installEventFilter(this);

    connect(textEdit_, &QTextEdit::cursorPositionChanged, this, [this]() {
        if (!inserting_) {
            completer_->popup()->hide();
            QToolTip::hideText();
        }
    });
}

void XmlCompleter::setConfigIdentifier(const QString &id)
{
    configIdentifier_ = id;
}

// -----------------------------------------------------------------------------
// Event filter
// -----------------------------------------------------------------------------

bool XmlCompleter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return QObject::eventFilter(obj, event);

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    QAbstractItemView *popup = completer_->popup();

    if (obj == popup) {
        switch (keyEvent->key()) {
        case Qt::Key_Tab:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            acceptCompletion();
            return true;
        case Qt::Key_Escape:
            popup->hide();
            return true;
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            return false; // let the popup navigate natively
        default:
            popup->hide();
            textEdit_->setFocus();
            QApplication::sendEvent(textEdit_, event);
            return true;
        }
    }

    if (obj != textEdit_)
        return QObject::eventFilter(obj, event);

    const bool altSpace = (keyEvent->modifiers() == Qt::AltModifier)
                          && keyEvent->key() == Qt::Key_Space;

    if (popup->isVisible()) {
        switch (keyEvent->key()) {
        case Qt::Key_Escape:
            popup->hide();
            return true;
        case Qt::Key_Tab:
            acceptCompletion();
            return true;
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            QApplication::sendEvent(popup, event);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            popup->hide();
            return false; // let the editor insert a newline
        default:
            popup->hide();
            break;
        }
    }

    if (altSpace) {
        triggerCompletion();
        return true;
    }

    const bool isTyping = !keyEvent->text().isEmpty()
                          && (keyEvent->modifiers() == Qt::NoModifier
                              || keyEvent->modifiers() == Qt::ShiftModifier);
    const bool isErase = (keyEvent->key() == Qt::Key_Backspace || keyEvent->key() == Qt::Key_Delete)
                         && (keyEvent->modifiers() == Qt::NoModifier
                             || keyEvent->modifiers() == Qt::ControlModifier);

    if (isTyping || isErase)
        QMetaObject::invokeMethod(this, &XmlCompleter::triggerCompletion, Qt::QueuedConnection);

    return false;
}

// -----------------------------------------------------------------------------
// Documentation tooltip
// -----------------------------------------------------------------------------

void XmlCompleter::onPopupCurrentChanged(const QModelIndex &current, const QModelIndex & /*previous*/)
{
    if (!current.isValid() || configIdentifier_.isEmpty()) {
        QToolTip::hideText();
        return;
    }

    const QString item = completer_->completionModel()->data(current).toString();
    if (item.isEmpty()) {
        QToolTip::hideText();
        return;
    }

    const auto docs = buildDocList(xsdStore_, configIdentifier_);
    QString hint;
    switch (currentCtx_.type) {
    case CompletionContext::Type::Element:
        hint = getElementDocumentation(item, docs);
        break;
    case CompletionContext::Type::Attribute:
        hint = getAttributeDocumentation(currentCtx_.elementName, item, docs);
        break;
    default:
        break;
    }

    if (hint.isEmpty()) {
        QToolTip::hideText();
        return;
    }

    // Wrap long hints so the tooltip stays readable.
    constexpr int kMaxLineLen = 80;
    if (hint.size() > kMaxLineLen) {
        QString wrapped;
        int lineLen = 0;
        for (const QChar c : hint) {
            if (c == ' ' && lineLen >= kMaxLineLen) {
                wrapped += '\n';
                lineLen = 0;
            } else {
                wrapped += c;
                ++lineLen;
                if (c == '\n')
                    lineLen = 0;
            }
        }
        hint = std::move(wrapped);
    }

    // Show tooltip to the right of the popup (falls back inside screen bounds automatically).
    QWidget *popup = completer_->popup();
    const QPoint pos = popup->mapToGlobal(QPoint(popup->width() + 4, 0));
    QToolTip::showText(pos, hint, popup);
}

// -----------------------------------------------------------------------------
// Completion lifecycle
// -----------------------------------------------------------------------------

void XmlCompleter::triggerCompletion()
{
    if (!textEdit_)
        return;

    const CompletionContext ctx = analyzeContext(
        textEdit_->toPlainText().left(textEdit_->textCursor().position()));

    if (ctx.type == CompletionContext::Type::None) {
        completer_->popup()->hide();
        return;
    }

    QStringList candidates;
    switch (ctx.type) {
    case CompletionContext::Type::Element:
        candidates = getElementNames(ctx.parentElementName);
        break;
    case CompletionContext::Type::Attribute:
        candidates = getAttributeNames(ctx.elementName);
        break;
    case CompletionContext::Type::Value:
        candidates = getAttributeValues(ctx.elementName, ctx.attributeName);
        break;
    case CompletionContext::Type::TextContent:
        candidates = getElementContentValues(ctx.elementName);
        break;
    default:
        break;
    }

    if (candidates.isEmpty()) {
        completer_->popup()->hide();
        return;
    }

    currentCtx_ = ctx;
    static_cast<QStringListModel *>(completer_->model())->setStringList(candidates);
    completer_->setCompletionPrefix(ctx.prefix);

    if (completer_->completionCount() == 0) {
        completer_->popup()->hide();
        return;
    }

    QRect cursorRect = textEdit_->cursorRect();
    cursorRect.setWidth(
        completer_->popup()->sizeHintForColumn(0)
        + completer_->popup()->verticalScrollBar()->sizeHint().width());
    completer_->complete(cursorRect);
}

void XmlCompleter::acceptCompletion()
{
    QAbstractItemView *popup = completer_->popup();
    QModelIndex idx = popup->currentIndex();
    if (!idx.isValid())
        idx = completer_->completionModel()->index(0, 0);

    const QString completion = completer_->completionModel()->data(idx).toString();
    suppressNextTrigger_ = true;
    if (!completion.isEmpty())
        insertCompletion(completion);

    QToolTip::hideText();
    popup->hide();
}

void XmlCompleter::insertCompletion(const QString &completion)
{
    if (!textEdit_)
        return;

    inserting_ = true;
    QTextCursor cursor = textEdit_->textCursor();
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, currentCtx_.prefix.length());
    cursor.removeSelectedText();
    cursor.insertText(completion);

    // Peek at the character immediately following the inserted text to avoid
    // doubling up characters the user may have already typed.
    QTextCursor peek = cursor;
    peek.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
    const QString next = peek.selectedText();

    switch (currentCtx_.type) {
    case CompletionContext::Type::Element:
        if (next != ">" && next != "/" && next != " ") {
            if (currentCtx_.isClosingTag) {
                cursor.insertText(">");
            } else {
                const auto docs = buildDocList(xsdStore_, configIdentifier_);
                cursor.insertText(isElementSelfClosing(completion, docs) ? "/>" : ">");
            }
        }
        break;

    case CompletionContext::Type::Attribute:
        if (next != "=") {
            cursor.insertText("=\"\"");
            cursor.movePosition(QTextCursor::Left);
        }
        break;

    case CompletionContext::Type::Value:
        if (next != "\"" && next != "'")
            cursor.insertText("\"");
        break;

    default:
        break;
    }

    textEdit_->setTextCursor(cursor);
    inserting_ = false;

    if (currentCtx_.type == CompletionContext::Type::Attribute)
        QMetaObject::invokeMethod(this, &XmlCompleter::triggerCompletion, Qt::QueuedConnection);
}

// -----------------------------------------------------------------------------
// Context analysis
// -----------------------------------------------------------------------------

XmlCompleter::CompletionContext XmlCompleter::analyzeContext(const QString &text) const
{
    // Walk backwards to find the nearest '<' (inside a tag) or '>' (text content).
    int tagStart = -1;
    int gtPos = -1;
    for (int i = text.size() - 1; i >= 0; --i) {
        if (text[i] == '>') {
            gtPos = i;
            break;
        }
        if (text[i] == '<') {
            tagStart = i;
            break;
        }
    }

    // -------------------------------------------------------------------------
    // Text content context (cursor is after a closing '>')
    // -------------------------------------------------------------------------
    if (tagStart == -1) {
        if (gtPos == -1)
            return {};

        // Recover the element name from the tag that produced this '>'.
        const int openBracket = text.lastIndexOf('<', gtPos - 1);
        if (openBracket == -1)
            return {};

        const QString tagStr = text.mid(openBracket + 1, gtPos - openBracket - 1);

        // Skip closing tags, processing instructions and comments.
        if (tagStr.startsWith('/') || tagStr.startsWith('?') || tagStr.startsWith('!'))
            return {};

        // Extract element name (first token before whitespace / '>' / '/').
        int nameEnd = tagStr.size();
        for (int i = 0; i < tagStr.size(); ++i) {
            if (tagStr[i].isSpace() || tagStr[i] == '>' || tagStr[i] == '/') {
                nameEnd = i;
                break;
            }
        }
        const QString element = tagStr.left(nameEnd);
        if (element.isEmpty())
            return {};

        // Prefix = text after the last separator (comma or whitespace).
        const QString content = text.mid(gtPos + 1);
        int sepPos = -1;
        for (int i = content.size() - 1; i >= 0; --i) {
            if (content[i] == ',' || content[i].isSpace()) {
                sepPos = i;
                break;
            }
        }

        CompletionContext ctx;
        ctx.type = CompletionContext::Type::TextContent;
        ctx.elementName = element;
        ctx.prefix = (sepPos >= 0) ? content.mid(sepPos + 1) : content;
        return ctx;
    }

    // -------------------------------------------------------------------------
    // Inside a tag
    // -------------------------------------------------------------------------
    const QString tagContent = text.mid(tagStart + 1);

    // Closing tag: </...
    if (tagContent.startsWith('/')) {
        CompletionContext ctx;
        ctx.type = CompletionContext::Type::Element;
        ctx.prefix = tagContent.mid(1);
        ctx.isClosingTag = true;
        ctx.parentElementName = findParentElement(text.left(tagStart));
        return ctx;
    }

    // Find the first whitespace separating element name from attributes.
    int spaceIdx = -1;
    for (int i = 0; i < tagContent.size(); ++i) {
        if (tagContent[i].isSpace()) {
            spaceIdx = i;
            break;
        }
    }

    // Still typing the element name.
    if (spaceIdx == -1) {
        CompletionContext ctx;
        ctx.type = CompletionContext::Type::Element;
        ctx.prefix = tagContent;
        ctx.parentElementName = findParentElement(text.left(tagStart));
        return ctx;
    }

    const QString elementName = tagContent.left(spaceIdx);
    const QString afterElement = tagContent.mid(spaceIdx);

    // Forward state machine: detect whether cursor is inside a quoted value.
    enum class QS { Outside, InDouble, InSingle };
    QS qstate = QS::Outside;
    int openQuotePos = -1;
    for (int i = 0; i < afterElement.size(); ++i) {
        const QChar c = afterElement[i];
        if (qstate == QS::Outside) {
            if (c == '"') {
                qstate = QS::InDouble;
                openQuotePos = i;
            } else if (c == '\'') {
                qstate = QS::InSingle;
                openQuotePos = i;
            }
        } else if (qstate == QS::InDouble && c == '"') {
            qstate = QS::Outside;
            openQuotePos = -1;
        } else if (qstate == QS::InSingle && c == '\'') {
            qstate = QS::Outside;
            openQuotePos = -1;
        }
    }

    if (qstate != QS::Outside) {
        // Value context: extract attribute name from the text before '='.
        const QString beforeEq = afterElement.left(openQuotePos).section('=', 0, -2);
        int end = beforeEq.size();
        while (end > 0 && beforeEq[end - 1].isSpace())
            --end;
        int start = end;
        while (start > 0 && !beforeEq[start - 1].isSpace())
            --start;
        const QString attrName = beforeEq.mid(start, end - start);
        if (attrName.isEmpty())
            return {};

        CompletionContext ctx;
        ctx.type = CompletionContext::Type::Value;
        ctx.elementName = elementName;
        ctx.attributeName = attrName;
        ctx.prefix = afterElement.mid(openQuotePos + 1);
        return ctx;
    }

    // Attribute name context: find the current token.
    int wordStart = afterElement.size();
    for (int i = afterElement.size() - 1; i >= 0; --i) {
        const QChar c = afterElement[i];
        if (c.isSpace() || c == '=' || c == '"' || c == '\'') {
            wordStart = i + 1;
            break;
        }
        if (i == 0)
            wordStart = 0;
    }

    // Cursor is right after a closing quote - nothing to complete.
    if (wordStart > 0) {
        const QChar prev = afterElement[wordStart - 1];
        if (prev == '=' || prev == '"' || prev == '\'')
            return {};
    }

    CompletionContext ctx;
    ctx.type = CompletionContext::Type::Attribute;
    ctx.elementName = elementName;
    ctx.prefix = afterElement.mid(wordStart);
    return ctx;
}

// -----------------------------------------------------------------------------
// XSD queries (public member functions, use the helpers above)
// -----------------------------------------------------------------------------

QStringList XmlCompleter::getElementNames(const QString &parentElement) const
{
    if (configIdentifier_.isEmpty())
        return {};
    const auto docs = buildDocList(xsdStore_, configIdentifier_);

    // Closing-tag completion: the only candidate is the element being closed.
    // (parentElementName holds the innermost unclosed element.)
    // This branch is reached when isClosingTag==true in the context;
    // we reuse parentElement for both cases to keep the interface simple.

    if (parentElement.isEmpty()) {
        // At document root: offer only the config root element.
        return {configIdentifier_};
    }

    // Query only the child elements declared directly inside the parent's
    // complexType. Named-type references (type="FooType") are NOT traversed,
    // so grandchildren stay invisible - exactly the behaviour we want.
    const QString base = elementTypeBaseXp(parentElement, docs);
    QStringList names = selectNames(docs, base + "//xs:element[@name]");
    names.removeDuplicates();
    return names;
}

QStringList XmlCompleter::getAttributeNames(const QString &elementName) const
{
    if (configIdentifier_.isEmpty() || elementName.isEmpty())
        return {};
    const auto docs = buildDocList(xsdStore_, configIdentifier_);
    return resolveAttributesFromBase(elementTypeBaseXp(elementName, docs), docs);
}

QStringList XmlCompleter::getAttributeValues(
    const QString &elementName, const QString &attributeName) const
{
    if (configIdentifier_.isEmpty() || elementName.isEmpty() || attributeName.isEmpty())
        return {};
    const auto docs = buildDocList(xsdStore_, configIdentifier_);
    const QString attrTypeName
        = findAttributeTypeFromBase(elementTypeBaseXp(elementName, docs), attributeName, docs);
    return attrTypeName.isEmpty() ? QStringList{} : getEnumerationValues(attrTypeName, docs);
}

QStringList XmlCompleter::getElementContentValues(const QString &elementName) const
{
    if (configIdentifier_.isEmpty() || elementName.isEmpty())
        return {};
    const auto docs = buildDocList(xsdStore_, configIdentifier_);
    const QString elemTypeName = resolveElementTypeName(elementName, docs);
    if (elemTypeName.isEmpty())
        return {};
    const QString contentTypeName = resolveElementContentTypeName(elemTypeName, docs);
    return contentTypeName.isEmpty() ? QStringList{}
                                     : getValuesFromSimpleType(contentTypeName, docs);
}
