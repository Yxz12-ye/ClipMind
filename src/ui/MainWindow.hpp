#pragma once

#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QStandardItemModel>
#include <QSystemTrayIcon>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

#include "./ContentListWidget.hpp"
#include "./CustomHead.hpp"
#include "./SearchWidget.hpp"
#include "./TagWidget.hpp"
#include "controller/UIController.hpp"
#include "service/ConfigService.hpp"
#include "service/EmbeddingService.hpp"

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
    bool settingsDialogOpen = false;
    bool hideAfterPaste = true;
    bool showTrayIcon = true;
    EmbeddingConfig embeddingConfig;
    bool hotkeyRegistered = false;
    unsigned int hotkeyModifiers = 0;
    unsigned int hotkeyVirtualKey = 0;
    QString hotkeyLabel;
    UIController* controller;
    ConfigService* config;

    void setupUI();
    void applyTheme();
    void refreshTagBar();
    void resetTagFilter();
    void setupTray();
    void setupGlobalHotkey();
    void teardownGlobalHotkey();
    void openSettings();
    void applySettings(const ApplicationSettings& settings, bool persist);
    QPoint resolveWindowPosition(quintptr caretThreadId) const;
    void showWindow(quintptr caretThreadId = 0);
    void hideWindow();
    void exitFromTray();
    static QPoint adjustWindowPositionToScreen(const QPoint& cursorPos, const QSize& windowSize,
                                               const QRect& availableGeometry);
    QScreen* getGlobalActiveWindowScreen() const;

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
