#include "MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QItemSelectionModel>
#include <QPalette>
#include <QScreen>
#include <QSettings>
#include <QStandardItem>
#include <QStringList>

#include "SettingsDialog.hpp"

#ifdef Q_OS_WIN
#include <WinUser.h>
#include <atlbase.h>
#include <atlsafe.h>
#include <oleacc.h>
#include <uiautomation.h>
#include <windows.h>

#pragma comment(lib, "Oleacc.lib")
#endif

#ifdef Q_OS_WIN
namespace {

QRect nativeGeometryForScreen(const QScreen* screen) {
    const QRect logicalGeometry = screen->geometry();
    const qreal devicePixelRatio = screen->devicePixelRatio();

    return QRect(logicalGeometry.topLeft(),
                 QSize(qRound(logicalGeometry.width() * devicePixelRatio),
                       qRound(logicalGeometry.height() * devicePixelRatio)));
}

QScreen* screenAtNativePoint(const QPoint& nativePoint) {
    const auto screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (nativeGeometryForScreen(screen).contains(nativePoint)) {
            return screen;
        }
    }

    return QGuiApplication::primaryScreen();
}

QPoint nativePointToLogicalPoint(const QPoint& nativePoint, const QScreen* screen) {
    const QRect logicalGeometry = screen->geometry();
    const qreal devicePixelRatio = screen->devicePixelRatio();
    const QPoint nativeOffset = nativePoint - logicalGeometry.topLeft();

    // Windows reports physical pixels, while Qt screen geometry uses logical pixels.
    return logicalGeometry.topLeft() + QPoint(qRound(nativeOffset.x() / devicePixelRatio),
                                              qRound(nativeOffset.y() / devicePixelRatio));
}

}  // namespace
#endif

void MainWindow::setupUI() {
    layout.setContentsMargins(QMargins(0, 0, 0, 0));
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

    central.setStyleSheet(QString("QWidget#centralPanel {"
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
                                  "}")
                              .arg(background, hover));
}

void MainWindow::setupTray() {
    const QIcon appIcon(":/img/icon.svg");
    setWindowIcon(appIcon);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    trayIcon.setIcon(appIcon);
    trayIcon.setToolTip(hotkeyLabel.isEmpty() ? QStringLiteral("ClipMind")
                                              : QStringLiteral("ClipMind (%1)").arg(hotkeyLabel));

    QAction* exitAction = trayMenu.addAction(QStringLiteral("退出"));
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitFromTray);
    connect(&trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                    showWindow();
                }
            });

    trayIcon.setContextMenu(&trayMenu);
    if (showTrayIcon) {
        trayIcon.show();
    }
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

HWND GetCaretPosEx(long* pX, long* pY, long* pW, long* pH);  // 引用声明

QScreen* MainWindow::getGlobalActiveWindowScreen() const {
#ifdef Q_OS_WIN
    // 1. 获取当前系统前台窗口（活动窗口）
    HWND hwnd = GetForegroundWindow();
    if (hwnd == nullptr) {
        return QGuiApplication::primaryScreen();
    }

    // 2. 获取该窗口的矩形区域
    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        return QGuiApplication::primaryScreen();
    }

    // 3. 取窗口中心点（比左上角更稳妥，避免窗口横跨多屏时定位到错误屏幕）
    POINT centerPoint;
    centerPoint.x = (windowRect.left + windowRect.right) / 2;
    centerPoint.y = (windowRect.top + windowRect.bottom) / 2;

    // 4. 通过中心点找到对应的 QScreen
    QScreen* screen = QGuiApplication::screenAt(QPoint(centerPoint.x, centerPoint.y));
    if (screen == nullptr) {
        return QGuiApplication::primaryScreen();
    }
    return screen;
#else
    // 非 Windows 平台退回到 Qt 内部方案
    QWidget* activeWidget = QApplication::activeWindow();
    return activeWidget ? activeWidget->screen() : QGuiApplication::primaryScreen();
#endif
}

QPoint MainWindow::resolveWindowPosition(quintptr caretThreadId) const {
    constexpr int kOffsetX = 24;
    constexpr int kOffsetY = 32;
    constexpr int kScreenMargin = 16;

    QPoint anchorPoint;
    QScreen* targetScreen = nullptr;

#ifdef Q_OS_WIN
    if (caretThreadId != 0) {
        GUITHREADINFO guiThreadInfo{};
        guiThreadInfo.cbSize = sizeof(GUITHREADINFO);
        if (GetGUIThreadInfo(static_cast<DWORD>(caretThreadId), &guiThreadInfo) &&
            guiThreadInfo.hwndCaret != nullptr) {
            RECT caretRect = guiThreadInfo.rcCaret;
            POINT caretPoint{caretRect.left, caretRect.bottom};
            if (ClientToScreen(guiThreadInfo.hwndCaret, &caretPoint)) {
                const QPoint nativeCaretPoint(caretPoint.x, caretPoint.y);
                targetScreen = screenAtNativePoint(nativeCaretPoint);
                anchorPoint = nativePointToLogicalPoint(nativeCaretPoint, targetScreen);
            }
        } else {
            long x = -1, y = -1, w = 0, h = 0;
            HWND hwnd = GetCaretPosEx(&x, &y, &w, &h);
            Q_UNUSED(hwnd);
            if (x != -1 && y != -1) {
                const QPoint nativeCaretPoint(x, y);
                targetScreen = screenAtNativePoint(nativeCaretPoint);
                anchorPoint = nativePointToLogicalPoint(nativeCaretPoint, targetScreen);
                // const QRect availableGeometry = targetScreen->availableGeometry();
                // return adjustWindowPositionToScreen(QPoint(x, y), size(), availableGeometry);
            }
        }
    }
#endif

    // ---- 鼠标回退逻辑（targetScreen == nullptr 分支） ----
    if (targetScreen == nullptr) {
        const QPoint cursorPosition = QCursor::pos();
        targetScreen = QGuiApplication::screenAt(cursorPosition);
        if (targetScreen == nullptr) {
            targetScreen = QGuiApplication::primaryScreen();
        }
        if (targetScreen == nullptr) {
            return pos();
        }

        const QRect availableGeometry = targetScreen->availableGeometry();

        // 调用剥离出的新函数，处理四个锚点候选和强制约束
        return adjustWindowPositionToScreen(cursorPosition, size(), availableGeometry);
    }

    // ---- 插入符逻辑（有目标屏幕） ----
    const QRect availableGeometry = targetScreen->availableGeometry();
    const int w = width();
    const int h = height();
    const int dx = kOffsetX;
    const int dy = kOffsetY;

    // 四个候选点（优先级：左上 → 左下 → 右上 → 右下）。偏移始终留在窗口外侧。
    QPoint candidates[4];
    candidates[0] = anchorPoint + QPoint(-dx - w + 1, -dy - h + 1);  // 窗口在插入符左上
    candidates[1] = anchorPoint + QPoint(-dx - w + 1, dy);           // 窗口在插入符左下
    candidates[2] = anchorPoint + QPoint(dx, -dy - h + 1);           // 窗口在插入符右上
    candidates[3] = anchorPoint + QPoint(dx, dy);                    // 窗口在插入符右下

    QPoint targetPoint;
    bool found = false;
    for (const QPoint& cand : candidates) {
        if (availableGeometry.contains(QRect(cand, size()))) {
            targetPoint = cand;
            found = true;
            break;
        }
    }

    if (!found) {
        // 所有候选都不合适，退回到强制约束（以 anchorPoint + 偏移为基准）
        QPoint fallback = anchorPoint + QPoint(dx, dy);
        targetPoint.setX(qBound(availableGeometry.left() + kScreenMargin, fallback.x(),
                                availableGeometry.right() - w - kScreenMargin));
        targetPoint.setY(qBound(availableGeometry.top() + kScreenMargin, fallback.y(),
                                availableGeometry.bottom() - h - kScreenMargin));
    }
    return targetPoint;
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

    QSettings settings;
    hideAfterPaste = settings.value("settings/hideAfterPaste", true).toBool();
    showTrayIcon = settings.value("settings/showTrayIcon", true).toBool();

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
    connect(&head, &CustomHead::settingsRequested, this, &MainWindow::openSettings);
    connect(&head, &CustomHead::moveRequested, this, [=](QPoint pos) { move(pos); });
    setupUI();

    contentList.setItems(controller->getCopyDate());
    connect(controller, &UIController::updateUI, this, &MainWindow::updateCopyList);
    connect(&searchWidget, &SearchWidget::inputTextChanged, controller,
            &UIController::requireSearch);
    connect(&contentList, &ContentListWidget::itemClicked, controller, &UIController::pasteContent);
    connect(controller, &UIController::hideWindowRequested, this, [this] {
        if (hideAfterPaste) {
            hideWindow();
        }
    });
}

MainWindow::~MainWindow() {
    teardownGlobalHotkey();
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        applyTheme();
    }

    if (event->type() == QEvent::ActivationChange && isVisible() && !isActiveWindow() &&
        !trayExitRequested && !settingsDialogOpen) {
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
        const HWND foregroundWindow = GetForegroundWindow();
        const DWORD foregroundThreadId =
            foregroundWindow != nullptr ? GetWindowThreadProcessId(foregroundWindow, nullptr) : 0;
        showWindow(static_cast<quintptr>(foregroundThreadId));
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

void MainWindow::showWindow(quintptr caretThreadId) {
#ifdef Q_OS_WIN
    if (caretThreadId == 0) {
        const HWND foregroundWindow = GetForegroundWindow();
        caretThreadId =
            foregroundWindow != nullptr
                ? static_cast<quintptr>(GetWindowThreadProcessId(foregroundWindow, nullptr))
                : 0;
    }
#endif

    QPoint _pos = resolveWindowPosition(caretThreadId);
    move(_pos);

    if (isMinimized()) {
        showNormal();
    } else {
        show();
    }
    contentList.scrollToTop();

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

void MainWindow::openSettings() {
    settingsDialogOpen = true;
    SettingsDialog dialog(hideAfterPaste, showTrayIcon, this);
    dialog.exec();
    settingsDialogOpen = false;

    hideAfterPaste = dialog.hideAfterPasteEnabled();
    showTrayIcon = dialog.trayIconEnabled();
    QSettings settings;
    settings.setValue("settings/hideAfterPaste", hideAfterPaste);
    settings.setValue("settings/showTrayIcon", showTrayIcon);
    trayIcon.setVisible(showTrayIcon);

    raise();
    activateWindow();
}

void MainWindow::exitFromTray() {
    trayExitRequested = true;
    teardownGlobalHotkey();
    trayIcon.hide();
    close();
    qApp->quit();
}

QPoint MainWindow::adjustWindowPositionToScreen(const QPoint& cursorPos, const QSize& windowSize,
                                                const QRect& availableGeometry) {
    const int w = windowSize.width();
    const int h = windowSize.height();

    // 以光标为基准，尝试窗口的四个角作为锚点
    // 左上角锚点：窗口左上角在光标处
    // 右上角锚点：窗口右上角在光标处
    // 左下角锚点：窗口左下角在光标处
    // 右下角锚点：窗口右下角在光标处
    const QPoint candidates[] = {
        cursorPos,                                             // 左上角
        QPoint(cursorPos.x() - w + 1, cursorPos.y()),          // 右上角
        QPoint(cursorPos.x(), cursorPos.y() - h + 1),          // 左下角
        QPoint(cursorPos.x() - w + 1, cursorPos.y() - h + 1),  // 右下角
    };

    for (const QPoint& candidate : candidates) {
        if (availableGeometry.contains(QRect(candidate, windowSize))) {
            return candidate;
        }
    }

    // 所有锚点都不合适，退化为强制约束（保证窗口不超出屏幕边界）
    const int maxX = qMax(availableGeometry.left(), availableGeometry.right() - w + 1);
    const int maxY = qMax(availableGeometry.top(), availableGeometry.bottom() - h + 1);

    return QPoint(qBound(availableGeometry.left(), cursorPos.x(), maxX),
                  qBound(availableGeometry.top(), cursorPos.y(), maxY));
}

void MainWindow::updateCopyList(QVector<ContentListItemData> data) {
    contentList.setItems(data);
}

/**
 * @ref https://www.autoahk.com/archives/44158
 */
HWND GetCaretPosEx(long* pX, long* pY, long* pW, long* pH) {
    CComPtr<IUIAutomation> uia;
    CComPtr<IUIAutomationElement> eleFocus;
    CComPtr<IUIAutomationValuePattern> valuePattern;
    if (S_OK != uia.CoCreateInstance(CLSID_CUIAutomation) || uia == nullptr) {
        return nullptr;
    }
    if (S_OK != uia->GetFocusedElement(&eleFocus) || eleFocus == nullptr) {
        goto useAccLocation;
    }
    if (S_OK == eleFocus->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&valuePattern)) &&
        valuePattern != nullptr) {
        BOOL isReadOnly;
        if (S_OK == valuePattern->get_CurrentIsReadOnly(&isReadOnly) && isReadOnly) {
            return nullptr;
        }
    }
useAccLocation:
    // use IAccessible::accLocation
    GUITHREADINFO guiThreadInfo = {sizeof(guiThreadInfo)};
    HWND hwndFocus = GetForegroundWindow();
    GetGUIThreadInfo(GetWindowThreadProcessId(hwndFocus, nullptr), &guiThreadInfo);
    hwndFocus = guiThreadInfo.hwndFocus ? guiThreadInfo.hwndFocus : hwndFocus;
    CComPtr<IAccessible> accCaret;
    if (S_OK == AccessibleObjectFromWindow(hwndFocus, OBJID_CARET, IID_PPV_ARGS(&accCaret)) &&
        accCaret != nullptr) {
        CComVariant varChild = CComVariant(0);
        if (S_OK == accCaret->accLocation(pX, pY, pW, pH, varChild)) {
            return hwndFocus;
        }
    }
    if (eleFocus == nullptr) {
        return nullptr;
    }
    // use IUIAutomationTextPattern2::GetCaretRange
    CComPtr<IUIAutomationTextPattern2> textPattern2;
    CComPtr<IUIAutomationTextRange> caretTextRange;
    CComSafeArray<double> rects;
    void* pVal = nullptr;
    BOOL IsActive = FALSE;
    if (S_OK != eleFocus->GetCurrentPatternAs(UIA_TextPattern2Id, IID_PPV_ARGS(&textPattern2)) ||
        textPattern2 == nullptr) {
        goto useGetSelection;
    }
    if (S_OK != textPattern2->GetCaretRange(&IsActive, &caretTextRange) ||
        caretTextRange == nullptr || !IsActive) {
        goto useGetSelection;
    }
    if (S_OK == caretTextRange->GetBoundingRectangles(rects.GetSafeArrayPtr()) &&
        rects != nullptr && SUCCEEDED(SafeArrayLock(rects)) && rects.GetCount() >= 4) {
        *pX = long(rects[0]);
        *pY = long(rects[1]);
        *pW = long(rects[2]);
        *pH = long(rects[3]);
        return hwndFocus;
    }
useGetSelection:
    // use IUIAutomationTextPattern::GetSelection
    CComPtr<IUIAutomationTextPattern> textPattern;
    CComPtr<IUIAutomationTextRangeArray> selectionRangeArray;
    CComPtr<IUIAutomationTextRange> selectionRange;
    if (textPattern2 == nullptr) {
        if (S_OK != eleFocus->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(&textPattern)) ||
            textPattern == nullptr) {
            return nullptr;
        }
    } else {
        textPattern = textPattern2;
    }
    if (S_OK != textPattern->GetSelection(&selectionRangeArray) || selectionRangeArray == nullptr) {
        return nullptr;
    }
    int length = 0;
    if (S_OK != selectionRangeArray->get_Length(&length) || length <= 0) {
        return nullptr;
    }
    if (S_OK != selectionRangeArray->GetElement(0, &selectionRange) || selectionRange == nullptr) {
        return nullptr;
    }
    if (S_OK != selectionRange->GetBoundingRectangles(rects.GetSafeArrayPtr()) ||
        rects == nullptr || FAILED(SafeArrayLock(rects))) {
        return nullptr;
    }
    if (rects.GetCount() < 4) {
        if (S_OK != selectionRange->ExpandToEnclosingUnit(TextUnit_Character)) {
            return nullptr;
        }
        if (S_OK != selectionRange->GetBoundingRectangles(rects.GetSafeArrayPtr()) ||
            rects == nullptr || FAILED(SafeArrayLock(rects)) || rects.GetCount() < 4) {
            return nullptr;
        }
    }
    *pX = long(rects[0]);
    *pY = long(rects[1]);
    *pW = long(rects[2]);
    *pH = long(rects[3]);
    return hwndFocus;
}
