//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once
#pragma once

#include <QString>

// Reads ProductName and ProductVersion from a PE file's string table.
// Returns empty strings for fields that are unavailable.
struct PeProductInfo
{
    QString name;
    QString version;
    QString description;
};

PeProductInfo peProductInfo(const QString &exePath);
