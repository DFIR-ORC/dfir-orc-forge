//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "xmlconfigeditor.h"

#include <comdef.h>
#include <msxml6.h>

#include <QFileDialog>
#include <QScrollbar>
#include <QTextDocument>
#include <QVBoxLayout>

#include "editor/xmlhighlighter.h"
#include "formatter/formatter.h"
#include "utils/guard.h"
#include "utils/log.h"
#include "xmldom.h"
#include "xsdstore.h"

namespace {

std::optional<QString> getConfigIdentifier(const QString &content)
{
    QRegularExpression elementRegex("<([a-zA-Z_:][a-zA-Z0-9_:.-]*)");

    QRegularExpressionMatch match = elementRegex.match(content);
    if (match.hasMatch()) {
        // The first capturing group contains the element name
        return match.captured(1);
    }

    return {};
}

// ---------------------------------------------------------------------------
// XSD-order pre-sort helpers
// ---------------------------------------------------------------------------

using XsdDocList = std::vector<MSXML::IXMLDOMDocument3Ptr>;

// Query all nodes matching xp and return their text values (lowercased).
// Returns the first doc that yields at least one match.
QStringList xsdSelectTexts(const XsdDocList &docs, const QString &xp)
{
    const bstr_t bxp(xp.toStdWString().c_str());
    for (const auto &doc : docs) {
        if (!doc)
            continue;
        MSXML::IXMLDOMNodeListPtr list;
        if (FAILED(doc->raw_selectNodes(bxp, &list)) || !list)
            continue;
        long len = 0;
        if (FAILED(list->get_length(&len)) || len == 0)
            continue;
        QStringList result;
        for (long i = 0; i < len; ++i) {
            MSXML::IXMLDOMNodePtr node;
            if (FAILED(list->get_item(i, &node)) || !node)
                continue;
            Guard::Bstr raw;
            if (FAILED(node->get_text(raw.addressOf())) || !raw)
                continue;
            result << QString::fromWCharArray(raw.get()).toLower();
        }
        if (!result.isEmpty())
            return result;
    }
    return {};
}

// XPath predicate that matches @name case-insensitively against nameLC.
QString ciNameFilter(const QString &nameLC)
{
    return QString("[translate(@name,"
                   "'ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz')='%1']")
        .arg(nameLC);
}

// Return the xs:sequence child element names (lowercased, in XSD document order)
// for the element whose name matches elementNameLC.
// Returns an empty list if the element has no xs:sequence content model
// (xs:all, xs:choice, simpleContent, or no children).
QStringList xsdSequenceOrder(const QString &elementNameLC, const XsdDocList &docs)
{
    const QString cf = ciNameFilter(elementNameLC);

    // Build sequence names from a base XPath (a xs:complexType node).
    // Captures both direct xs:element children of xs:sequence AND xs:element
    // children of xs:choice nodes that are direct children of xs:sequence.
    // The XPath union | preserves document order, so the returned names match
    // the sequence position of each element (including those inside xs:choice).
    auto seqNames = [&](const QString &base) {
        const QString xp = QString("%1/xs:sequence/xs:element/@name"
                                   " | %1/xs:sequence/xs:choice/xs:element/@name")
                               .arg(base);
        return xsdSelectTexts(docs, xp);
    };

    // Inline xs:complexType
    {
        auto names = seqNames(QString("//xs:element%1[1]/xs:complexType").arg(cf));
        if (!names.isEmpty())
            return names;
    }

    // Named type: first resolve the @type attribute, then query that complexType
    const QStringList typeNames = xsdSelectTexts(docs, QString("//xs:element%1[1]/@type").arg(cf));
    if (!typeNames.isEmpty()) {
        auto names = seqNames(QString("//xs:complexType%1[1]").arg(ciNameFilter(typeNames.first())));
        if (!names.isEmpty())
            return names;
    }

    return {};
}

// Recursively sort an XML element's direct children to match the XSD xs:sequence
// order. Elements not listed in the sequence are moved to the end (stable).
// xs:all / xs:choice types return an empty order list and are left untouched.
void sortByXsdSequence(MSXML::IXMLDOMNodePtr node, const XsdDocList &docs)
{
    if (!node)
        return;
    MSXML::DOMNodeType nt = MSXML::NODE_INVALID;
    if (FAILED(node->get_nodeType(&nt)) || nt != MSXML::NODE_ELEMENT)
        return;

    Guard::Bstr rawName;
    if (FAILED(node->get_nodeName(rawName.addressOf())) || !rawName)
        return;
    const QString elemName = QString::fromWCharArray(rawName.get()).toLower();

    // Collect element children into a vector
    MSXML::IXMLDOMNodeListPtr children;
    if (FAILED(node->get_childNodes(&children)) || !children)
        return;
    long total = 0;
    if (FAILED(children->get_length(&total)))
        return;

    std::vector<MSXML::IXMLDOMNodePtr> elems;
    for (long i = 0; i < total; ++i) {
        MSXML::IXMLDOMNodePtr child;
        if (FAILED(children->get_item(i, &child)) || !child)
            continue;
        MSXML::DOMNodeType ct = MSXML::NODE_INVALID;
        if (FAILED(child->get_nodeType(&ct)))
            continue;
        if (ct == MSXML::NODE_ELEMENT)
            elems.push_back(child);
    }

    // Sort against the XSD sequence order (no-op when order is empty)
    const QStringList order = xsdSequenceOrder(elemName, docs);
    if (!order.isEmpty() && elems.size() > 1) {
        std::stable_sort(
            elems.begin(),
            elems.end(),
            [&](const MSXML::IXMLDOMNodePtr &a, const MSXML::IXMLDOMNodePtr &b) {
                Guard::Bstr ra, rb;
                a->get_nodeName(ra.addressOf());
                b->get_nodeName(rb.addressOf());
                const QString na = ra ? QString::fromWCharArray(ra.get()).toLower() : QString();
                const QString nb = rb ? QString::fromWCharArray(rb.get()).toLower() : QString();
                const int ia = order.indexOf(na);
                const int ib = order.indexOf(nb);
                return (ia < 0 ? order.size() : ia) < (ib < 0 ? order.size() : ib);
            });
        // Re-insert in sorted order; appendChild moves an already-present node
        for (auto &child : elems) {
            MSXML::IXMLDOMNodePtr discarded;
            node->raw_appendChild(child, &discarded);
        }
    }

    // Recurse with a fresh child list (node order may have changed above)
    MSXML::IXMLDOMNodeListPtr fresh;
    if (FAILED(node->get_childNodes(&fresh)) || !fresh)
        return;
    long newLen = 0;
    if (FAILED(fresh->get_length(&newLen)))
        return;
    for (long i = 0; i < newLen; ++i) {
        MSXML::IXMLDOMNodePtr child;
        if (FAILED(fresh->get_item(i, &child)) || !child)
            continue;
        sortByXsdSequence(child, docs);
    }
}

} // namespace

XmlConfigEditor::XmlConfigEditor(XsdStore &xsdStore, QWidget *parent)
    : TextEditWidget(parent)
    , xsdStore_(xsdStore)
    , dom_(std::make_unique<XmlDom>())
{
    connect(
        editor_->document(),
        &QTextDocument::contentsChange,
        &editorSignals(),
        &EditorInterface::Signals::contentsChanged);

    connect(
        editor_->document(),
        &QTextDocument::contentsChange,
        this,
        [this](int position, int charsRemoved, int charsAdded) {
            if (charsRemoved > 0 || charsAdded > 0) {
                status_->clear();
                validationTimer_.singleShot(500, this, &XmlConfigEditor::validate);
            }
        });

    new XmlHighlighter(editor_->document());
}

XmlConfigEditor::~XmlConfigEditor() {}

bool XmlConfigEditor::load(std::optional<QString> path)
{
    if (!path) {
        path = QFileDialog::getOpenFileName(
            this, "Open File", "", "All Files (*.*);;XML configuration file (*.xml)");
    }

    QFile file(*path);
    if (!file.open(QIODevice::ReadWrite)) {
        LOG_CRITICAL("Failed to open file (path: {})", *path);
        return false;
    }

    editor_->setPlainText(QString::fromUtf8(file.readAll()));
    path_ = path;
    return true;
}

bool XmlConfigEditor::load(const QByteArray &data, EditorFormat format)
{
    if (format != EditorFormat::XmlConfiguration && format != EditorFormat::Unspecified) {
        return false;
    }

    editor_->setPlainText(QString::fromUtf8(data));
    return true;
}

bool XmlConfigEditor::save()
{
    auto path = path_;
    if (!path) {
        return saveAs();
    }

    if (!saveToFile(*path)) {
        return false;
    }

    return true;
}

bool XmlConfigEditor::saveAs()
{
    auto path = QFileDialog::getSaveFileName(
        this, "Save file", "", QString("File (*.xml);; All Files (*.*)"));
    if (path.isEmpty()) {
        return false;
    }

    if (!saveToFile(path)) {
        return false;
    }

    return true;
}

std::expected<QByteArray, std::error_code> XmlConfigEditor::content() const
{
    return editor_->toPlainText().toUtf8();
}

bool XmlConfigEditor::saveToFile(QString path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_CRITICAL("Failed to open file to save (path: {})", path);
        return false;
    }

    auto data = editor_->toPlainText().toUtf8();
    if (file.write(data) != data.size()) {
        return false;
    }

    path_ = path;
    emit editorSignals().fileSaved(path);
    return true;
}

std::optional<QString> XmlConfigEditor::path() const
{
    return path_;
}

std::optional<QString> XmlConfigEditor::getConfigIdentifier()
{
    return configIdentifier_;
}

void XmlConfigEditor::validate()
{
    QString content = editor_->toPlainText().toLower();

    auto id = ::getConfigIdentifier(content);
    if (!id) {
        status_->setText("Configuration identifier not found", StatusType::Error);
        return;
    }

    auto xsd = xsdStore_.get(*id);
    if (!xsd) {
        status_->setText(QString("Missing schema for '%1'").arg(*id), StatusType::Error);
        return;
    }

    if (*id != configIdentifier_) {
        configIdentifier_ = *id;
        emit configIdentifierChanged(*id);
    }

    auto document = dom_->loadFromBuffer(content);
    if (!document) {
        status_->setText(document.error(), StatusType::Error);
        return;
    }

    auto xml = document.value();

    // Build a list of XSD documents to drive the sort.
    XsdDocList xsdDocs;
    if (auto d = xsdStore_.get(*id))
        xsdDocs.push_back(*d);
    if (auto common = xsdStore_.getCommonDocument())
        xsdDocs.push_back(common);

    // Pre-sort child elements to match the XSD xs:sequence order before
    // passing the document to MSXML6. This lets the user write elements in
    // any order without triggering sequence-order validation errors.
    // Only the in-memory DOM is modified; the editor text is not touched.
    if (!xsdDocs.empty()) {
        MSXML::IXMLDOMElementPtr rootElem;
        if (SUCCEEDED(xml->get_documentElement(&rootElem)) && rootElem) {
            // IXMLDOMElement inherits IXMLDOMNode - safe static upcast
            MSXML::IXMLDOMNodePtr rootNode(
                static_cast<MSXML::IXMLDOMNode *>(rootElem.GetInterfacePtr()));
            sortByXsdSequence(rootNode, xsdDocs);
        }
    }

    auto schema = xsdStore_.getNormalizedSchema();
    if (!schema) {
        status_->setText("No schema available", StatusType::Error);
        return;
    }

    HRESULT hr = xml->putref_schemas(variant_t(schema));
    if (FAILED(hr)) {
        status_->setText("Cannot use schema", StatusType::Error);
        return;
    }

    MSXML::IXMLDOMParseErrorPtr parseError;
    hr = xml->raw_validate(&parseError);
    if (FAILED(hr)) {
        if (parseError) {
            status_->setText(parseError, StatusType::Error);
        } else {
            status_->setText("Unknown issue", StatusType::Error);
        }
    } else if (hr == S_FALSE) {
        if (parseError) {
            status_->setText(parseError, StatusType::Warning);
        } else {
            status_->setText("Unknown issue", StatusType::Warning);
        }
    } else {
        status_->setText("Validated with schema", StatusType::Success);
    }
}
