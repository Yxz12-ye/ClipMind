#include<QApplication>
#include"./ui/MainWindow.hpp"

int main(int argc, char** argv){
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    MainWindow w;
    return app.exec();
}
