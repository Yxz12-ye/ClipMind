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
signals:
    void onCopyEventTrigered();
};

#ifdef WIN32
#include <windows.h>
#include <winuser.h>
/**
 * @ref https://learn.microsoft.com/zh-cn/windows/win32/dataxchg/clipboard
 */
class WindowsCopyEventListener : public AbstractCopyEventListener
{
private:
    HWND hwnd;
    bool registerListenService() override;
public:
    WindowsCopyEventListener(QWidget* topWidget, QObject* parent = nullptr);
    ~WindowsCopyEventListener() = default;
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


