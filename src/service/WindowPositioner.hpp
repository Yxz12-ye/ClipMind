#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>

/**
 * 窗口定位的统一接口.
 *
 * 各平台判断"窗口应该出现在哪里"的依据不同:
 *  - Windows: 优先跟随前台输入焦点(插入符), 取不到时退化为鼠标位置;
 *  - 其他平台: 暂时统一放到屏幕右下角.
 *
 * 调用方只需要给出窗口尺寸, 平台差异全部封装在实现里.
 */
class AbstractWindowPositioner {
public:
    /// 窗口与屏幕边缘保持的安全间距
    static constexpr int kScreenMargin = 16;

    virtual ~AbstractWindowPositioner() = default;

    /**
     * 计算窗口左上角应当所处的位置.
     * @param windowSize 待定位窗口的尺寸
     */
    virtual QPoint resolvePosition(const QSize& windowSize) const = 0;
};

/**
 * 通用兜底策略: 把窗口放到屏幕右下角.
 */
class ScreenCornerWindowPositioner : public AbstractWindowPositioner {
public:
    /**
     * 把 windowSize 大小的窗口放到 availableGeometry 的右下角, 并留出 kScreenMargin 间距.
     * 窗口比屏幕还大时, 退化为可用区域的左上角.
     */
    static QPoint bottomRightOf(const QSize& windowSize, const QRect& availableGeometry);

    QPoint resolvePosition(const QSize& windowSize) const override;
};

#ifdef Q_OS_WIN
/**
 * Windows 策略: 优先贴着输入框的插入符弹出, 取不到插入符时跟着鼠标走.
 */
class WindowsWindowPositioner : public AbstractWindowPositioner {
public:
    QPoint resolvePosition(const QSize& windowSize) const override;
};
#endif

/// 创建当前平台的窗口定位实现, 调用方负责释放
AbstractWindowPositioner* createWindowPositioner();
