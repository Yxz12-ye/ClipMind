#include "CopyEventListener.hpp"

#ifdef WIN32

bool WindowsCopyEventListener::registerListenService() 
{
    if (AddClipboardFormatListener(hwnd)){
        // 此处需要接入Debug信息
        qDebug()<<"成功添加剪贴板监听";
    }
    else{
        // 此处需要接入Debug信息
        qDebug()<<"无法添加剪贴板监听";
        return false;
    }
    return true;
}

bool WindowsCopyEventListener::handleNativeEvent(void* message, qintptr* result) {
    MSG *msg = reinterpret_cast<MSG*>(message);
    if (msg->message == WM_CLIPBOARDUPDATE) {
        // 剪贴板已更新，发出信号
        emit clipboardChanged();
        if (result) *result = 0;
        return true; // 消息已处理
    }
    Q_UNUSED(result);
    return false;
}

WindowsCopyEventListener::WindowsCopyEventListener(QObject* parent)
    : AbstractCopyEventListener(parent) {
    m_hiddenWindow = new HiddenWindow(nullptr);
    m_hiddenWindow->setAttribute(Qt::WA_NativeWindow);
    m_hiddenWindow->setGeometry(0, 0, 1, 1);
    m_hiddenWindow->setVisible(false);
    m_hiddenWindow->m_listener = this;

    hwnd = reinterpret_cast<HWND>(m_hiddenWindow->winId());
    connect(this, &AbstractCopyEventListener::clipboardChanged, this, &WindowsCopyEventListener::getClipboardText);
    registerListenService();
}

WindowsCopyEventListener::~WindowsCopyEventListener() {
    if (m_hiddenWindow) m_hiddenWindow->deleteLater();
    if (hwnd) RemoveClipboardFormatListener(hwnd);
}

void WindowsCopyEventListener::getClipboardText() {
    if (!OpenClipboard(hwnd)) return;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT); // 获取句柄
        if (hData) {
            wchar_t* pwchData = (wchar_t*)GlobalLock(hData); // 锁定内存
            if (pwchData) {
                lastText = QString::fromWCharArray(pwchData); // 复制数据
                GlobalUnlock(hData); // 解锁内存
            }
        }
    }
    CloseClipboard();
    return;
}

HiddenWindow::HiddenWindow(QWidget* parent) :QWidget(parent) {}

bool HiddenWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    if (m_listener && m_listener->handleNativeEvent(message, result)) return true;
    return QWidget::nativeEvent(eventType, message, result);
}

#endif

QString AbstractCopyEventListener::text() {
    return lastText;
}
