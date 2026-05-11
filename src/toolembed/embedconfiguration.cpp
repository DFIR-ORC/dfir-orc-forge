//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include "embedconfiguration.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include "xmldom.h"

std::expected<QString, QString> FindEmbedConfiguration(const QString &folder)
{
    const QDir dir(folder);

    // 1. Prefer an explicit embed.xml
    const QString embedXml = dir.filePath("embed.xml");
    if (QFileInfo::exists(embedXml)) {
        return embedXml;
    }

    // 2. Scan all XML files for one whose root element is <toolembed>
    const QStringList xmlFiles = dir.entryList(QStringList{"*.xml"}, QDir::Files | QDir::Readable);

    QStringList candidates;
    for (const QString &fileName : xmlFiles) {
        const QString filePath = dir.filePath(fileName);
        XmlDom dom;
        if (!dom.loadFromFile(filePath)) {
            continue;
        }
        auto root = dom.getRootElement();
        if (!root) {
            continue;
        }
        _bstr_t tagName;
        if (FAILED((*root)->get_tagName(tagName.GetAddress()))) {
            continue;
        }
        if (QString::fromWCharArray(static_cast<const wchar_t *>(tagName))
                .compare("toolembed", Qt::CaseInsensitive)
            == 0) {
            candidates.append(filePath);
        }
    }

    if (candidates.size() == 1) {
        return candidates.first();
    }

    if (candidates.size() > 1) {
        return std::unexpected(
            QString("Multiple toolembed configuration files found in folder:\n%1")
                .arg(candidates.join("\n")));
    }

    return std::unexpected(
        QString("No toolembed configuration file found in folder:\n%1").arg(folder));
}
