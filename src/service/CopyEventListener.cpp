#include "CopyEventListener.hpp"

#include <QDebug>
#include <cstring>

#ifdef Q_OS_WIN

bool WindowsCopyEventListener::registerListenService() {
    if (AddClipboardFormatListener(hwnd)) {
        qDebug() << "成功添加剪贴板监听";
        return true;
    }

    qDebug() << "无法添加剪贴板监听";
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
        qWarning() << "无法打开剪贴板";
        return false;
    }

    if (!EmptyClipboard()) {
        qWarning() << "无法清空剪贴板";
        CloseClipboard();
        return false;
    }

    const SIZE_T size = static_cast<SIZE_T>(text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (memory == nullptr) {
        qWarning() << "无法分配剪贴板内存";
        CloseClipboard();
        return false;
    }

    auto* destination = static_cast<wchar_t*>(GlobalLock(memory));
    if (destination == nullptr) {
        qWarning() << "无法锁定剪贴板内存";
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    std::memcpy(destination, text.utf16(), size);
    GlobalUnlock(memory);

    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        qWarning() << "无法写入剪贴板";
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    m_pastedClipboardSequenceNumber = GetClipboardSequenceNumber();
    return true;
}

void WindowsCopyEventListener::pasteText(const QString& text) {
    if (!writeClipboardText(text)) {
        return;
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
        qWarning() << "无法发送粘贴快捷键";
    }
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

void AbstractCopyEventListener::pasteText(const QString& text) {
    Q_UNUSED(text);
}

AbstractCopyEventListener* createCopyEventListener(QObject* parent) {
#ifdef Q_OS_WIN
    return new WindowsCopyEventListener(parent);
#else
    Q_UNUSED(parent);
    return nullptr;
#endif
}
