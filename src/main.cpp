#include <QApplication>

#ifdef Q_OS_WIN
#include <objbase.h>
#endif

#include "./service/LogService.hpp"
#include "./ui/MainWindow.hpp"
#include "config.hpp"

int main(int argc, char** argv) {
#ifdef Q_OS_WIN
    const HRESULT comInitializationResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
    int exitCode = 0;
    {
        QApplication app(argc, argv);
        app.setQuitOnLastWindowClosed(false);

        // 日志落在系统临时目录: Debug 构建记录全部日志, Release 构建只记录关键日志
        LogService::initialize();
        LogService::info("main", "{} {} started, log file: {}", QStringLiteral(PROJECT_NAME),
                         QStringLiteral(PROJECT_VERSION), LogService::logFilePath());

        MainWindow w;
        exitCode = app.exec();

        LogService::info("main", "exiting, return code: {}", exitCode);
        LogService::shutdown();
    }
#ifdef Q_OS_WIN
    if (SUCCEEDED(comInitializationResult)) {
        CoUninitialize();
    }
#endif
    return exitCode;
}
