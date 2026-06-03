#pragma once

#include <QObject>
#include <QWidget>

class AbstractCopyEventListener : public QObject
{
    Q_OBJECT
private:
    virtual bool registerListenService();
    

public:
    AbstractCopyEventListener(QObject* parent = nullptr);
    virtual ~AbstractCopyEventListener() = default;

    QString lastText;
    QString text();
    
signals:
    void clipboardChanged();
public slots:
    virtual void getClipboardText();
};

#ifdef WIN32
#include <windows.h>
#include <winuser.h>
/**
 * @ref https://learn.microsoft.com/zh-cn/windows/win32/dataxchg/clipboard
 */
class WindowsCopyEventListener : public AbstractCopyEventListener
{
    Q_OBJECT
private:
    HiddenWindow* m_hiddenWindow;
    HWND hwnd;
    bool registerListenService() override;
private slots:
    void getClipboardText() override;

public:
    WindowsCopyEventListener(QObject* parent = nullptr);
    ~WindowsCopyEventListener();

    bool handleNativeEvent(void *message, qintptr *result);
    
};

class HiddenWindow : public QWidget
{
private:
public:
    WindowsCopyEventListener* m_listener;
    HiddenWindow(QWidget* parent = nullptr);
    ~HiddenWindow() = default;

    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
};

#endif

class PALCopyEventListener
{
private:
    /* data */
public:
    PALCopyEventListener(/* args */);
    ~PALCopyEventListener();
};


