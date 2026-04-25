#pragma once

#include <QMainWindow>
#include <QWidget>
#include "./CustomHead.hpp"

#include <QVBoxLayout>

class MainWindow : public QMainWindow {
private:
    QWidget central;
    CustomHead head;
    QVBoxLayout layout;

    void setupUI();
    void applyTheme();

public:
    MainWindow(/* args */);
    ~MainWindow()=default;

protected:
    void changeEvent(QEvent* event) override;
};
