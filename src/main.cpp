//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright 2025-2026 ANSSI. All Rights Reserved.
//
// Author(s): fabienfl (ANSSI)
//

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QMainWindow>
#include <QTimer>

#include <QApplication>

#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <windows.h>

#include "mainwindow.h"
#include "utils/log.h"

// --- logging helpers -------------------------------------------------------

static QtMsgType g_logLevel = QtWarningMsg;

void setLogLevel(QtMsgType level)
{
    g_logLevel = level;
}

void installConsoleMessageHandler()
{
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &msg) {
        if (type < g_logLevel) {
            return;
        }
        const char *prefix = "";
        switch (type) {
        case QtDebugMsg:
            prefix = "[D] ";
            break;
        case QtInfoMsg:
            prefix = "[I] ";
            break;
        case QtWarningMsg:
            prefix = "[W] ";
            break;
        case QtCriticalMsg:
            prefix = "[E] ";
            break;
        case QtFatalMsg:
            prefix = "[C] ";
            break;
        }
        fprintf(stderr, "%s%s\n", prefix, qPrintable(msg));
        fflush(stderr);
    });
}

// Attach to an existing console (e.g. PowerShell / cmd) or allocate a new one,
// then redirect the CRT stderr handle so fprintf reaches it.
static void attachConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }
    // Reopen CRT stderr onto the Win32 console handle.
    FILE *fp = nullptr;
    freopen_s(&fp, "CONOUT$", "w", stderr);
}

// ---------------------------------------------------------------------------

class COMManager
{
public:
    static COMManager &getInstance()
    {
        static COMManager instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    COMManager(const COMManager &) = delete;
    COMManager &operator=(const COMManager &) = delete;

private:
    COMManager()
    {
        HRESULT hr = CoInitializeSecurity(
            NULL,
            -1,                          // COM negotiates service
            NULL,                        // Authentication services
            NULL,                        // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
            NULL,                        // Authentication info
            EOAC_NONE,                   // Additional capabilities
            NULL                         // Reserved
        );

        if (FAILED(hr)) {
            LOG_CRITICAL("Failed CoInitializeSecurity");
            return;
        }
    }

    ~COMManager() { CoUninitialize(); }
};

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char *lpCmdLine, int nCmdShow)
{
    COMManager::getInstance();

    QApplication app(__argc, __argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Configuration editor for DFIR-ORC");
    parser.addHelpOption();
    parser.addPositionalArgument("arg", "Path to a configured DFIR-ORC binary");

    const QCommandLineOption logLevelOption(
        {"log-level", "l"},
        "Minimum log level to print to the console (debug, info, warning, critical).",
        "level");
    parser.addOption(logLevelOption);

    parser.process(app);

    if (parser.isSet(logLevelOption)) {
        const QString levelStr = parser.value(logLevelOption).toLower();
        QtMsgType level = QtWarningMsg;
        if (levelStr == "debug") {
            level = QtDebugMsg;
        } else if (levelStr == "info") {
            level = QtInfoMsg;
        } else if (levelStr == "warning") {
            level = QtWarningMsg;
        } else if (levelStr == "critical") {
            level = QtCriticalMsg;
        } else {
            fprintf(
                stderr,
                "Unknown log level '%s'. Valid values: debug, info, warning, critical.\n",
                qPrintable(levelStr));
        }
        attachConsole();
        setLogLevel(level);
        installConsoleMessageHandler();
    }

    MainWindow mainWindow;

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        QTimer::singleShot(0, [&]() { mainWindow.loadFile(args.first()); });
    }

    mainWindow.show();

    return app.exec();
}
