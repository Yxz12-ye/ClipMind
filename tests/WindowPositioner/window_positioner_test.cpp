#include <QGuiApplication>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <gtest/gtest.h>
#include <memory>

#include "service/WindowPositioner.hpp"

TEST(ScreenCornerWindowPositionerTest, PlacesWindowAtBottomRightWithMargin) {
    const QRect available(0, 0, 1920, 1040);
    const QSize window(360, 400);

    const QPoint position = ScreenCornerWindowPositioner::bottomRightOf(window, available);

    // 屏幕右下角再留出 kScreenMargin 间距
    EXPECT_EQ(position, QPoint(1920 - 360 - AbstractWindowPositioner::kScreenMargin,
                               1040 - 400 - AbstractWindowPositioner::kScreenMargin));
    EXPECT_TRUE(available.contains(QRect(position, window)));
}

TEST(ScreenCornerWindowPositionerTest, RespectsOffsetAvailableGeometry) {
    // 副屏的可用区域可以带偏移, 也可能是负数坐标
    const QRect available(1920, 180, 1280, 900);
    const QSize window(360, 400);

    const QPoint position = ScreenCornerWindowPositioner::bottomRightOf(window, available);

    EXPECT_EQ(position, QPoint(available.left() + available.width() - window.width() -
                                   AbstractWindowPositioner::kScreenMargin,
                               available.top() + available.height() - window.height() -
                                   AbstractWindowPositioner::kScreenMargin));
    EXPECT_TRUE(available.contains(QRect(position, window)));
}

TEST(ScreenCornerWindowPositionerTest, ClampsOversizedWindowToTopLeft) {
    // 窗口比可用区域还大时不能算出屏幕外的坐标
    const QRect available(0, 0, 320, 240);
    const QSize window(360, 400);

    EXPECT_EQ(ScreenCornerWindowPositioner::bottomRightOf(window, available), available.topLeft());
}

TEST(WindowPositionerFactoryTest, ResolvesPositionInsideSomeScreen) {
    std::unique_ptr<AbstractWindowPositioner> positioner(createWindowPositioner());
    ASSERT_NE(positioner, nullptr);

    if (QGuiApplication::primaryScreen() == nullptr) {
        GTEST_SKIP() << "当前环境没有可用屏幕";
    }

    const QPoint position = positioner->resolvePosition(QSize(360, 400));

    bool onSomeScreen = false;
    const auto screens = QGuiApplication::screens();
    for (const QScreen* screen : screens) {
        if (screen->geometry().contains(position)) {
            onSomeScreen = true;
            break;
        }
    }
    EXPECT_TRUE(onSomeScreen) << "窗口左上角 " << position.x() << "," << position.y()
                              << " 不在任何屏幕内";
}

int main(int argc, char** argv) {
    // 定位实现只依赖 QtGui, 因此用 QGuiApplication 就够, 不必拉起 Widgets
    QGuiApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
