#include "WindowPositioner.hpp"

#include <QGuiApplication>
#include <QScreen>

#ifdef Q_OS_WIN
#include <QCursor>

// clang-format off
// windows.h 必须排在其余 Windows 头之前: 它负责定义 _AMD64_ 等目标架构宏,
// 否则 winnt.h 会直接 #error "No Target Architecture".
#include <windows.h>
#include <WinUser.h>
#include <atlbase.h>
#include <atlsafe.h>
#include <oleacc.h>
#include <uiautomation.h>
// clang-format on

#pragma comment(lib, "Oleacc.lib")

namespace {

/// 插入符与窗口之间留出的偏移量, 让窗口不要盖住正在输入的位置
constexpr int kCaretOffsetX = 24;
constexpr int kCaretOffsetY = 32;

/// Windows 报告的是物理像素, 而 Qt 的屏幕几何使用逻辑像素, 这里先换算成物理像素再比较
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

    return logicalGeometry.topLeft() + QPoint(qRound(nativeOffset.x() / devicePixelRatio),
                                              qRound(nativeOffset.y() / devicePixelRatio));
}

/// 当前前台窗口所属的 GUI 线程, 用来查询该线程的插入符信息
quintptr foregroundThreadId() {
    const HWND foregroundWindow = GetForegroundWindow();
    return foregroundWindow != nullptr
               ? static_cast<quintptr>(GetWindowThreadProcessId(foregroundWindow, nullptr))
               : 0;
}

/// 以光标为基准选择窗口位置, 四个角都放不下时强制约束在屏幕内
QPoint adjustWindowPositionToScreen(const QPoint& cursorPos, const QSize& windowSize,
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

}  // namespace
#endif

QPoint ScreenCornerWindowPositioner::bottomRightOf(const QSize& windowSize,
                                                   const QRect& availableGeometry) {
    const int x = availableGeometry.right() - windowSize.width() + 1 - kScreenMargin;
    const int y = availableGeometry.bottom() - windowSize.height() + 1 - kScreenMargin;

    // 窗口比可用区域还大时, 上面的结果会跑到区域外, 这里夹回左上角
    return QPoint(qMax(availableGeometry.left(), x), qMax(availableGeometry.top(), y));
}

QPoint ScreenCornerWindowPositioner::resolvePosition(const QSize& windowSize) const {
    const QScreen* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        return QPoint();
    }

    return bottomRightOf(windowSize, screen->availableGeometry());
}

#ifdef Q_OS_WIN
QPoint WindowsWindowPositioner::resolvePosition(const QSize& windowSize) const {
    QPoint anchorPoint;
    QScreen* targetScreen = nullptr;

    // ---- 插入符优先: 让窗口贴着用户正在输入的位置弹出 ----
    const quintptr caretThreadId = foregroundThreadId();
    if (caretThreadId != 0) {
        GUITHREADINFO guiThreadInfo{};
        guiThreadInfo.cbSize = sizeof(GUITHREADINFO);
        if (GetGUIThreadInfo(static_cast<DWORD>(caretThreadId), &guiThreadInfo) &&
            guiThreadInfo.hwndCaret != nullptr) {
            RECT caretRect = guiThreadInfo.rcCaret;
            POINT caretPoint{caretRect.left, caretRect.bottom};
            if (ClientToScreen(guiThreadInfo.hwndCaret, &caretPoint)) {
                const QPoint nativeCaretPoint(caretPoint.x, caretPoint.y);
                QScreen* caretScreen = screenAtNativePoint(nativeCaretPoint);
                if (caretScreen != nullptr) {
                    targetScreen = caretScreen;
                    anchorPoint = nativePointToLogicalPoint(nativeCaretPoint, caretScreen);
                }
            }
        } else {
            long x = -1, y = -1, w = 0, h = 0;
            HWND hwnd = GetCaretPosEx(&x, &y, &w, &h);
            Q_UNUSED(hwnd);
            if (x != -1 && y != -1) {
                const QPoint nativeCaretPoint(x, y);
                QScreen* caretScreen = screenAtNativePoint(nativeCaretPoint);
                if (caretScreen != nullptr) {
                    targetScreen = caretScreen;
                    anchorPoint = nativePointToLogicalPoint(nativeCaretPoint, caretScreen);
                }
            }
        }
    }

    // ---- 鼠标回退: 拿不到插入符时跟着光标走 ----
    if (targetScreen == nullptr) {
        const QPoint cursorPosition = QCursor::pos();
        targetScreen = QGuiApplication::screenAt(cursorPosition);
        if (targetScreen == nullptr) {
            targetScreen = QGuiApplication::primaryScreen();
        }
        if (targetScreen == nullptr) {
            return QPoint();
        }

        return adjustWindowPositionToScreen(cursorPosition, windowSize,
                                            targetScreen->availableGeometry());
    }

    // ---- 候选位置: 以插入符为锚点, 四个角都试一遍 ----
    const QRect availableGeometry = targetScreen->availableGeometry();
    const int w = windowSize.width();
    const int h = windowSize.height();

    // 四个候选点（优先级：左上 → 左下 → 右上 → 右下）。偏移始终留在窗口外侧。
    const QPoint candidates[4] = {
        anchorPoint + QPoint(-kCaretOffsetX - w + 1, -kCaretOffsetY - h + 1),  // 窗口在插入符左上
        anchorPoint + QPoint(-kCaretOffsetX - w + 1, kCaretOffsetY),           // 窗口在插入符左下
        anchorPoint + QPoint(kCaretOffsetX, -kCaretOffsetY - h + 1),           // 窗口在插入符右上
        anchorPoint + QPoint(kCaretOffsetX, kCaretOffsetY),                    // 窗口在插入符右下
    };

    for (const QPoint& candidate : candidates) {
        if (availableGeometry.contains(QRect(candidate, windowSize))) {
            return candidate;
        }
    }

    // 所有候选都不合适，退回到强制约束（以 anchorPoint + 偏移为基准）
    const QPoint fallback = anchorPoint + QPoint(kCaretOffsetX, kCaretOffsetY);
    return QPoint(qBound(availableGeometry.left() + kScreenMargin, fallback.x(),
                         availableGeometry.right() - w - kScreenMargin),
                  qBound(availableGeometry.top() + kScreenMargin, fallback.y(),
                         availableGeometry.bottom() - h - kScreenMargin));
}
#endif

AbstractWindowPositioner* createWindowPositioner() {
#ifdef Q_OS_WIN
    return new WindowsWindowPositioner();
#else
    return new ScreenCornerWindowPositioner();
#endif
}
