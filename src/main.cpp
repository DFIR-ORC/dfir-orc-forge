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

#include <windows.h>

#include "mainwindow.h"
#include "utils/log.h"

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
    parser.process(app);

    MainWindow mainWindow;

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        QTimer::singleShot(0, [&]() { mainWindow.loadFile(args.first()); });
    }

    mainWindow.show();

    return app.exec();
}
