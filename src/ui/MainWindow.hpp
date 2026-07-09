#pragma once

#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QStandardItemModel>
#include <QSystemTrayIcon>
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
    static constexpr int kHotkeyId = 1;

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
    bool hotkeyRegistered = false;
    unsigned int hotkeyModifiers = 0;
    unsigned int hotkeyVirtualKey = 0;
    QString hotkeyLabel;

    void setupUI();
    void applyTheme();
    void setupTray();
    void setupGlobalHotkey();
    void teardownGlobalHotkey();
    QPoint resolveWindowPosition() const;
    void showWindow();
    void hideWindow();
    void exitFromTray();

    UIController* controller;

public:
    MainWindow(/* args */);
    ~MainWindow() override;

protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

public slots:
    void updateCopyList(QVector<ContentListItemData> data);
};
