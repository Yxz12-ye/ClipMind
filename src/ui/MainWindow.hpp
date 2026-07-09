#pragma once

#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QWidget>

#include "./ContentListWidget.hpp"
#include "./CustomHead.hpp"
#include "./SearchWidget.hpp"
#include "./TagWidget.hpp"
#include "controller/UIController.hpp"

class QCloseEvent;

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
    QMenu trayMenu;
    QSystemTrayIcon trayIcon;
    bool trayExitRequested = false;

    void setupUI();
    void applyTheme();
    void setupTray();
    void populateDemoData();
    void showFromTray();
    void exitFromTray();

    UIController* controller;

public:
    MainWindow(/* args */);
    ~MainWindow() = default;

protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

public slots:
    void updateCopyList(QVector<ContentListItemData> data);
};
