//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <expected>

#include <QString>

// Scans a folder for a DFIR-ORC toolembed configuration file.
// Looks for embed.xml first; if not found, scans all XML files for one whose
// root element is <toolembed>. Returns the resolved file path on success, or
// an error message if no file or multiple files are found.
std::expected<QString, QString> FindEmbedConfiguration(const QString &folder);
