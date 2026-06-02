#include "CopyEventListener.hpp"

#ifdef WIN32

#endif

bool WindowsCopyEventListener::registerListenService() 
{
    if (AddClipboardFormatListener(hwnd)){
        // 此处需要接入Debug信息
    }
    else{
        // 此处需要接入Debug信息
    }
}

WindowsCopyEventListener::WindowsCopyEventListener(QWidget* topWidget, QObject* parent)
    : AbstractCopyEventListener(parent) {
    hwnd = reinterpret_cast<HWND>(topWidget->winId());
    registerListenService();
}