#include "MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QIcon>
#include <QItemSelectionModel>
#include <QPalette>
#include <QStandardItem>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

void MainWindow::setupUI() {
    layout.setContentsMargins(QMargins(0,0,0,0));
    layout.setSpacing(0);
    layout.addWidget(&head);
    layout.addSpacing(8);
    layout.addWidget(&searchWidget);
    layout.addSpacing(12);
    layout.addWidget(&tagContainer, 0, Qt::AlignHCenter);
    layout.addSpacing(12);
    layout.addWidget(&contentList, 1);
}

void MainWindow::applyTheme() {
    const bool darkMode = palette().color(QPalette::Window).lightness() < 128;
    const QString background = darkMode ? "#1C1C1C" : "#FFFFFF";
    const QString hover = darkMode ? "rgba(255, 255, 255, 0.08)" : "rgba(15, 23, 42, 0.06)";

    central.setStyleSheet(QString(
        "QWidget#centralPanel {"
        "background-color: %1;"
        "border-radius: 12px;"
        "}"
        "QWidget#centralPanel QToolButton {"
        "border: none;"
        "border-radius: 9px;"
        "background: transparent;"
        "}"
        "QWidget#centralPanel QToolButton:hover {"
        "background-color: %2;"
        "}"
    ).arg(background, hover));
}

void MainWindow::setupTray() {
    const QIcon appIcon(":/img/icon.svg");
    setWindowIcon(appIcon);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    trayIcon.setIcon(appIcon);
    trayIcon.setToolTip(hotkeyLabel.isEmpty() ? QStringLiteral("ClipMind") : QStringLiteral("ClipMind (%1)").arg(hotkeyLabel));

    QAction* exitAction = trayMenu.addAction(QStringLiteral("退出"));
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitFromTray);
    connect(&trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showWindow();
        }
    });

    trayIcon.setContextMenu(&trayMenu);
    trayIcon.show();
}

void MainWindow::setupGlobalHotkey() {
#ifdef Q_OS_WIN
    const WId windowId = winId();
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (hwnd == nullptr) {
        return;
    }

    struct HotkeyCandidate {
        unsigned int modifiers;
        unsigned int virtualKey;
        const wchar_t* label;
    };

    const HotkeyCandidate candidates[] = {
        {MOD_ALT | MOD_NOREPEAT, 'V', L"Alt+V"},
        {MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'V', L"Ctrl+Alt+V"},
    };

    for (const HotkeyCandidate& candidate : candidates) {
        if (RegisterHotKey(hwnd, kHotkeyId, candidate.modifiers, candidate.virtualKey)) {
            hotkeyRegistered = true;
            hotkeyModifiers = candidate.modifiers;
            hotkeyVirtualKey = candidate.virtualKey;
            hotkeyLabel = QString::fromWCharArray(candidate.label);
            return;
        }
    }
#endif
}

void MainWindow::teardownGlobalHotkey() {
#ifdef Q_OS_WIN
    if (!hotkeyRegistered) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd != nullptr) {
        UnregisterHotKey(hwnd, kHotkeyId);
    }
#endif

    hotkeyRegistered = false;
    hotkeyModifiers = 0;
    hotkeyVirtualKey = 0;
    hotkeyLabel.clear();
}

MainWindow::MainWindow()
    : central(this),
      head(&central),
      searchWidget(&central),
      tagContainer(&central),
      tagListView(&central),
      contentList(&central),
      model(new QStandardItemModel(this)),
      tagContainerLayout(&tagContainer),
      layout(&central),
      trayMenu(this),
      trayIcon(this),
      controller(new UIController(this)) {
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(360, 400);
    central.setObjectName("centralPanel");
    central.setAttribute(Qt::WA_StyledBackground, true);

    tagContainer.setFixedSize(328, 28);
    tagContainer.setAttribute(Qt::WA_StyledBackground, true);
    tagContainer.setStyleSheet("QWidget { background: transparent; }");
    tagContainerLayout.setContentsMargins(0, 0, 0, 0);
    tagContainerLayout.setSpacing(0);
    tagContainerLayout.addWidget(&tagListView, 0, Qt::AlignCenter);
    tagContainerLayout.addStretch();

    tagListView.setModel(model);
    tagListView.setSelectionMode(QAbstractItemView::SingleSelection);
    tagListView.setStyleSheet("QListView { background: transparent; }");

    applyTheme();
    setupGlobalHotkey();
    setupTray();
    setCentralWidget(&central);
    connect(&head, &CustomHead::closeRequested, this, &QWidget::close);
    connect(&head, &CustomHead::moveRequested, this, [=](QPoint pos){move(pos);});
    setupUI();

    contentList.setItems(controller->getCopyDate());
    connect(controller, &UIController::updateUI, this, &MainWindow::updateCopyList);
}

MainWindow::~MainWindow() {
    teardownGlobalHotkey();
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        applyTheme();
    }

    if (event->type() == QEvent::ActivationChange && isVisible() && !isActiveWindow() && !trayExitRequested) {
        hideWindow();
    }

    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (trayExitRequested || !trayIcon.isVisible()) {
        QMainWindow::closeEvent(event);
        return;
    }

    hideWindow();
    event->ignore();
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(eventType);
#ifdef Q_OS_WIN
    MSG* msg = reinterpret_cast<MSG*>(message);
    if (msg != nullptr && msg->message == WM_HOTKEY && msg->wParam == kHotkeyId) {
        showWindow();
        if (result != nullptr) {
            *result = 0;
        }
        return true;
    }
#else
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif

    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::showWindow() {
    if (isMinimized()) {
        showNormal();
    } else {
        show();
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd != nullptr) {
        SetForegroundWindow(hwnd);
    }
#endif

    raise();
    activateWindow();
    searchWidget.focusInput();
}

void MainWindow::hideWindow() {
    hide();
}

void MainWindow::exitFromTray() {
    trayExitRequested = true;
    teardownGlobalHotkey();
    trayIcon.hide();
    close();
    qApp->quit();
}

void MainWindow::updateCopyList(QVector<ContentListItemData> data) {
    contentList.setItems(data);
}
