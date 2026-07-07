#pragma once

#include <QHBoxLayout>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QWidget>

#include "./ContentListWidget.hpp"
#include "./CustomHead.hpp"
#include "./SearchWidget.hpp"
#include "./TagWidget.hpp"
#include "controller/UIController.hpp"

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

    UIController* controller;

public:
    MainWindow(/* args */);
    ~MainWindow() = default;

protected:
    void changeEvent(QEvent* event) override;

public slots:
    void updateCopyList(QVector<ContentListItemData> data);
};
