#include "CopyEventListener.hpp"

#include <QDebug>

#ifdef Q_OS_WIN

bool WindowsCopyEventListener::registerListenService()
{
    if (AddClipboardFormatListener(hwnd)) {
        qDebug() << "成功添加剪贴板监听";
        return true;
    }

    qDebug() << "无法添加剪贴板监听";
    return false;
}

bool WindowsCopyEventListener::handleNativeEvent(void* message, qintptr* result)
{
    MSG* msg = reinterpret_cast<MSG*>(message);
    if (msg->message == WM_CLIPBOARDUPDATE) {
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
    : AbstractCopyEventListener(parent),
      m_hiddenWindow(new HiddenWindow(nullptr)),
      hwnd(nullptr)
{
    m_hiddenWindow->setAttribute(Qt::WA_NativeWindow);
    m_hiddenWindow->setGeometry(0, 0, 1, 1);
    m_hiddenWindow->setVisible(false);
    m_hiddenWindow->m_listener = this;

    hwnd = reinterpret_cast<HWND>(m_hiddenWindow->winId());
    registerListenService();
}

WindowsCopyEventListener::~WindowsCopyEventListener()
{
    if (hwnd) {
        RemoveClipboardFormatListener(hwnd);
        hwnd = nullptr;
    }

    delete m_hiddenWindow;
    m_hiddenWindow = nullptr;
}

void WindowsCopyEventListener::getClipboardText()
{
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

HiddenWindow::HiddenWindow(QWidget* parent)
    : QWidget(parent),
      m_listener(nullptr)
{}

bool HiddenWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (m_listener && m_listener->handleNativeEvent(message, result)) {
        return true;
    }

    return QWidget::nativeEvent(eventType, message, result);
}

#endif

AbstractCopyEventListener::AbstractCopyEventListener(QObject* parent)
    : QObject(parent)
{}

QString AbstractCopyEventListener::text() const
{
    return lastText;
}

AbstractCopyEventListener* createCopyEventListener(QObject* parent)
{
#ifdef Q_OS_WIN
    return new WindowsCopyEventListener(parent);
#else
    Q_UNUSED(parent);
    return nullptr;
#endif
}
