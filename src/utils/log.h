//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <format>
#include <QDebug>

#define LOG_DEBUG(fmt, ...) qDebug() << std::format(fmt, __VA_ARGS__).c_str()
#define LOG_WARN(fmt, ...) qWarning() << std::format(fmt, __VA_ARGS__).c_str()
#define LOG_CRITICAL(fmt, ...) qCritical() << std::format(fmt, __VA_ARGS__).c_str()
