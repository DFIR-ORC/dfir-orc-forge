//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//
#pragma once

#include <cstdio>
#include <format>
#include <QDebug>
#include <QMessageLogContext>
#include <QString>

#define LOG_DEBUG(fmt, ...) qDebug() << std::format(fmt, ##__VA_ARGS__).c_str()
#define LOG_INFO(fmt, ...) qInfo() << std::format(fmt, ##__VA_ARGS__).c_str()
#define LOG_WARN(fmt, ...) qWarning() << std::format(fmt, ##__VA_ARGS__).c_str()
#define LOG_CRITICAL(fmt, ...) qCritical() << std::format(fmt, ##__VA_ARGS__).c_str()

// Set the minimum log level for the console message handler.
// Accepted values: QtDebugMsg, QtInfoMsg, QtWarningMsg, QtCriticalMsg.
void setLogLevel(QtMsgType level);

// Install a message handler that writes filtered log lines to stderr.
// Call after setLogLevel() and after the console has been attached.
void installConsoleMessageHandler();
