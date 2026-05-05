#pragma once

#include <QMainWindow>
#include <QWidget>
#include "./CustomHead.hpp"
#include "./TagWidget.hpp"
#include "./SearchWidget.hpp"
#include <QStandardItemModel>

#include <QVBoxLayout>

class MainWindow : public QMainWindow {
private:
    QWidget central;
    CustomHead head;
    SearchWidget searchWidget;
    TagListView tagListView;
    QStandardItemModel* model;
    QVBoxLayout layout;

    void setupUI();
    void applyTheme();

public:
    MainWindow(/* args */);
    ~MainWindow()=default;

protected:
    void changeEvent(QEvent* event) override;
};
