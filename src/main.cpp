#include<QApplication>
#include"./ui/MainWindow.hpp"

int main(int argc, char** argv){
#ifdef Q_OS_WIN
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    MainWindow w;
#ifdef Q_OS_WIN
    CoUninitialize();
#endif
    return app.exec();
}
