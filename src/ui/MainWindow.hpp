#pragma once

#include <QMainWindow>
#include <QWidget>
#include "./CustomHead.hpp"
#include "./TagWidget.hpp"
#include "./SearchWidget.hpp"
#include "./ContentListWidget.hpp"
#include <QStandardItemModel>

#include <QHBoxLayout>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
private:
    QWidget central;
    CustomHead head;
    SearchWidget searchWidget;
    QWidget tagContainer;
    TagListView tagListView;
    ContentListWidget contentList;
    QStandardItemModel* model;
    QHBoxLayout tagContainerLayout;
    QVBoxLayout layout;

    void setupUI();
    void applyTheme();
    void populateDemoData();

public:
    MainWindow(/* args */);
    ~MainWindow()=default;

protected:
    void changeEvent(QEvent* event) override;
};
