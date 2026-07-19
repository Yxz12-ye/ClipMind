#pragma once

#include <QObject>
#include <QWidget>

class AbstractCopyEventListener : public QObject {
    Q_OBJECT
private:
    virtual bool registerListenService() = 0;

public:
    AbstractCopyEventListener(QObject* parent = nullptr);
    virtual ~AbstractCopyEventListener() = default;

    QString lastText;
    QString text() const;

signals:
    void clipboardChanged();
public slots:
    virtual void getClipboardText() = 0;
    virtual bool pasteText(const QString& text);
};

#ifdef Q_OS_WIN
#include <windows.h>
#include <winuser.h>

class HiddenWindow;

/**
 * @ref https://learn.microsoft.com/zh-cn/windows/win32/dataxchg/clipboard
 */
class WindowsCopyEventListener : public AbstractCopyEventListener {
    Q_OBJECT
private:
    HiddenWindow* m_hiddenWindow;
    HWND hwnd;
    DWORD m_pastedClipboardSequenceNumber = 0;
    bool registerListenService() override;
    bool writeClipboardText(const QString& text);
private slots:
    void getClipboardText() override;
    bool pasteText(const QString& text) override;

public:
    WindowsCopyEventListener(QObject* parent = nullptr);
    ~WindowsCopyEventListener();

    bool handleNativeEvent(void* message, qintptr* result);
};

class HiddenWindow : public QWidget {
private:
public:
    WindowsCopyEventListener* m_listener;
    HiddenWindow(QWidget* parent = nullptr);
    ~HiddenWindow() = default;

    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
};

#elif Q_OS_LINUX

#elif Q_OS_MACOS

#endif

AbstractCopyEventListener* createCopyEventListener(QObject* parent = nullptr);
