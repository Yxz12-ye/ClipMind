#include "CopyEventListener.hpp"

#include <cstring>

#include "LogService.hpp"

#ifdef Q_OS_WIN

bool WindowsCopyEventListener::registerListenService() {
    if (AddClipboardFormatListener(hwnd)) {
        LogService::debug("CopyEventListener", "clipboard listener added");
        return true;
    }

    LogService::warn("CopyEventListener", "failed to add clipboard listener");
    return false;
}

bool WindowsCopyEventListener::handleNativeEvent(void* message, qintptr* result) {
    MSG* msg = reinterpret_cast<MSG*>(message);
    if (msg->message == WM_CLIPBOARDUPDATE) {
        if (GetClipboardSequenceNumber() == m_pastedClipboardSequenceNumber) {
            m_pastedClipboardSequenceNumber = 0;
            if (result) {
                *result = 0;
            }
            return true;
        }

        getClipboardText();
        emit clipboardChanged();
        if (result) {
            *result = 0;
        }
        return true;
    }

    Q_UNUSED(result);
    return false;
}

WindowsCopyEventListener::WindowsCopyEventListener(QObject* parent)
    : AbstractCopyEventListener(parent), m_hiddenWindow(new HiddenWindow(nullptr)), hwnd(nullptr) {
    m_hiddenWindow->setAttribute(Qt::WA_NativeWindow);
    m_hiddenWindow->setGeometry(0, 0, 1, 1);
    m_hiddenWindow->setVisible(false);
    m_hiddenWindow->m_listener = this;

    hwnd = reinterpret_cast<HWND>(m_hiddenWindow->winId());
    registerListenService();
}

WindowsCopyEventListener::~WindowsCopyEventListener() {
    if (hwnd) {
        RemoveClipboardFormatListener(hwnd);
        hwnd = nullptr;
    }

    delete m_hiddenWindow;
    m_hiddenWindow = nullptr;
}

void WindowsCopyEventListener::getClipboardText() {
    lastText.clear();
    if (!OpenClipboard(hwnd)) {
        return;
    }

    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pwchData = static_cast<wchar_t*>(GlobalLock(hData));
            if (pwchData) {
                lastText = QString::fromWCharArray(pwchData);
                GlobalUnlock(hData);
            }
        }
    }

    CloseClipboard();
}

bool WindowsCopyEventListener::writeClipboardText(const QString& text) {
    if (!OpenClipboard(hwnd)) {
        LogService::warn("CopyEventListener", "failed to open clipboard");
        return false;
    }

    if (!EmptyClipboard()) {
        LogService::warn("CopyEventListener", "failed to clear clipboard");
        CloseClipboard();
        return false;
    }

    const SIZE_T size = static_cast<SIZE_T>(text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (memory == nullptr) {
        LogService::warn("CopyEventListener", "failed to allocate clipboard memory");
        CloseClipboard();
        return false;
    }

    auto* destination = static_cast<wchar_t*>(GlobalLock(memory));
    if (destination == nullptr) {
        LogService::warn("CopyEventListener", "failed to lock clipboard memory");
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    std::memcpy(destination, text.utf16(), size);
    GlobalUnlock(memory);

    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        LogService::warn("CopyEventListener", "failed to write clipboard data");
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    m_pastedClipboardSequenceNumber = GetClipboardSequenceNumber();
    return true;
}

bool WindowsCopyEventListener::pasteText(const QString& text) {
    if (!writeClipboardText(text)) {
        return false;
    }

    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    if (SendInput(4, inputs, sizeof(INPUT)) != 4) {
        LogService::warn("CopyEventListener", "failed to send paste shortcut");
        return false;
    }

    return true;
}

HiddenWindow::HiddenWindow(QWidget* parent) : QWidget(parent), m_listener(nullptr) {}

bool HiddenWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    if (m_listener && m_listener->handleNativeEvent(message, result)) {
        return true;
    }

    return QWidget::nativeEvent(eventType, message, result);
}

#endif

AbstractCopyEventListener::AbstractCopyEventListener(QObject* parent) : QObject(parent) {}

QString AbstractCopyEventListener::text() const {
    return lastText;
}

bool AbstractCopyEventListener::pasteText(const QString& text) {
    Q_UNUSED(text);
    return false;
}

AbstractCopyEventListener* createCopyEventListener(QObject* parent) {
#ifdef Q_OS_WIN
    return new WindowsCopyEventListener(parent);
#else
    Q_UNUSED(parent);
    return nullptr;
#endif
}
