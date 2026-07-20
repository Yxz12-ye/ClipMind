#include <QApplication>

#ifdef Q_OS_WIN
#include <objbase.h>
#endif

#include "./ui/MainWindow.hpp"

int main(int argc, char** argv) {
#ifdef Q_OS_WIN
    const HRESULT comInitializationResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
    int exitCode = 0;
    {
        QApplication app(argc, argv);
        app.setQuitOnLastWindowClosed(false);
        MainWindow w;
        exitCode = app.exec();
    }
#ifdef Q_OS_WIN
    if (SUCCEEDED(comInitializationResult)) {
        CoUninitialize();
    }
#endif
    return exitCode;
}
