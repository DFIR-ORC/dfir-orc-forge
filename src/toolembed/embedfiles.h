//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <expected>
#include <functional>
#include <QList>
#include <QSet>
#include <QString>

// One <file path="..."> entry from the toolembed configuration.
// Represents any third-party executable to embed, with or without a download URL.
//
// When url= is present it has the format:
//   <url>[,type=auto|zip|7z][,item=path/inside/archive.exe][,eula=eula.txt]
//
// type=auto (default) -- detect from magic bytes after download
// type=zip / type=7z  -- force a specific codec
// item=               -- file to extract; defaults to basename of name=
// eula=               -- explicit EULA file inside the archive; when omitted,
//                        the archive is scanned automatically for well-known
//                        license filenames (eula.txt, license.txt, copying, ...)
struct EmbedFile
{
    QString name;        // value of the name= attribute
    QString dest;        // resolved absolute destination path
    bool presentOnDisk;  // true when dest already exists
    QString url;         // raw value of the url= attribute (empty when absent)
    QString urlParsed;   // parsed URL part (before first ',')
    QString archiveType; // "zip", "7z", "auto", or "" (auto)
    QString item;        // parsed item= value, or "" (default: basename of name=)
    QString eula;        // parsed eula= value, or "" (no EULA required)
};

// Callback invoked when a EULA must be shown to the user.
// Receives the EULA text and the display name of the executable it applies to;
// returns true if the user accepts, false to decline.
using EulaPromptFn = std::function<bool(const QString &eulaText, const QString &executableName)>;

// Returns all executable <file path="..."> entries declared in the toolembed
// config, including entries without a url= attribute (presentOnDisk set
// accordingly, urlParsed and related fields left empty).
// Does not perform any network I/O.
// Returns an error message if the config file cannot be parsed.
std::expected<QList<EmbedFile>, QString> ListEmbedFiles(
    const QString &embedConfigPath, const QString &configFolder);

// Downloads all entries from ListEmbedFiles that have a url= attribute,
// writing them to their resolved paths (relative to configFolder).
// Skips files that already exist on disk unless their dest path appears in
// forceDests, in which case they are re-downloaded and overwritten.
// When a EULA file is declared (eula=) and its content has not yet been
// accepted, eulaPrompt is called; if it returns false the entry is skipped
// and an error is returned.
// Returns an error message if any download fails or a EULA is declined.
std::expected<void, QString> ResolveEmbedDownloads(
    const QString &embedConfigPath,
    const QString &configFolder,
    EulaPromptFn eulaPrompt = {},
    QSet<QString> forceDests = {});

// Extracts the RT_RCDATA/"DFIRORC_CONFIG" archive embedded in the current
// executable into <tempBaseDir>/dfir-orc-config/ and returns the first
// top-level subdirectory found inside it (e.g. config/).  Falls back to the
// extraction root if no subdirectory is present.
// Returns an error message if the resource is absent or extraction fails.
std::expected<QString, QString> ExtractEmbeddedConfig(const QString &tempBaseDir);
