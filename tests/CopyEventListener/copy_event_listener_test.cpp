#include <gtest/gtest.h>

#include <QApplication>
#include <QString>

#include "service/CopyEventListener.hpp"

#ifdef Q_OS_WIN
#include <windows.h>

#include <cstring>
#include <optional>
#include <string>
#endif

class FakeCopyEventListener : public AbstractCopyEventListener
{
    Q_OBJECT

private:
    bool registerListenService() override { return true; }

public:
    using AbstractCopyEventListener::AbstractCopyEventListener;

    void getClipboardText() override
    {
        lastText = nextText_;
    }

    void primeClipboardText(const QString& text)
    {
        nextText_ = text;
    }

private:
    QString nextText_;
};

TEST(CopyEventListenerBaseTest, TextReturnsCachedValue)
{
    FakeCopyEventListener listener;
    listener.lastText = QStringLiteral("cached");

    EXPECT_EQ(listener.text(), QStringLiteral("cached"));
}

TEST(CopyEventListenerBaseTest, FakeListenerUpdatesCachedText)
{
    FakeCopyEventListener listener;
    listener.primeClipboardText(QStringLiteral("updated"));

    listener.getClipboardText();

    EXPECT_EQ(listener.text(), QStringLiteral("updated"));
}

TEST(CopyEventListenerFactoryTest, FactoryMatchesPlatformContract)
{
    QObject parent;
    AbstractCopyEventListener* listener = createCopyEventListener(&parent);

#ifdef Q_OS_WIN
    ASSERT_NE(listener, nullptr);
    EXPECT_EQ(listener->parent(), &parent);
#else
    EXPECT_EQ(listener, nullptr);
#endif

    delete listener;
}

#ifdef Q_OS_WIN

namespace
{
std::optional<QString> readClipboardText()
{
    if (!OpenClipboard(nullptr)) {
        return std::nullopt;
    }

    std::optional<QString> text;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            const auto* raw = static_cast<const wchar_t*>(GlobalLock(hData));
            if (raw) {
                text = QString::fromWCharArray(raw);
                GlobalUnlock(hData);
            }
        }
    }

    CloseClipboard();
    return text;
}

bool writeClipboardText(const QString& text)
{
    if (!OpenClipboard(nullptr)) {
        return false;
    }

    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }

    const std::wstring wide = text.toStdWString();
    const SIZE_T bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) {
        CloseClipboard();
        return false;
    }

    void* target = GlobalLock(memory);
    if (!target) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    memcpy(target, wide.c_str(), bytes);
    GlobalUnlock(memory);

    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

bool clearClipboard()
{
    if (!OpenClipboard(nullptr)) {
        return false;
    }

    const bool cleared = EmptyClipboard();
    CloseClipboard();
    return cleared;
}
}  // namespace

class WindowsCopyEventListenerIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        originalClipboardText_ = readClipboardText();
        listener_ = dynamic_cast<WindowsCopyEventListener*>(createCopyEventListener());
        ASSERT_NE(listener_, nullptr);
    }

    void TearDown() override
    {
        delete listener_;
        listener_ = nullptr;

        if (originalClipboardText_.has_value()) {
            writeClipboardText(*originalClipboardText_);
        } else {
            clearClipboard();
        }
    }

    bool dispatchClipboardUpdate(qintptr* result = nullptr)
    {
        MSG msg{};
        msg.message = WM_CLIPBOARDUPDATE;
        return listener_->handleNativeEvent(&msg, result);
    }

    bool dispatchMessage(UINT message, qintptr* result = nullptr)
    {
        MSG msg{};
        msg.message = message;
        return listener_->handleNativeEvent(&msg, result);
    }

    WindowsCopyEventListener* listener_ = nullptr;
    std::optional<QString> originalClipboardText_;
};

TEST_F(WindowsCopyEventListenerIntegrationTest, ClipboardUpdateReadsRealClipboardTextBeforeEmittingSignal)
{
    const QString expected = QStringLiteral("hello from win32 clipboard");
    ASSERT_TRUE(writeClipboardText(expected));

    QString observed;
    int signalCount = 0;
    QObject::connect(listener_, &AbstractCopyEventListener::clipboardChanged,
                     [this, &observed, &signalCount]() {
                         ++signalCount;
                         observed = listener_->text();
                     });

    qintptr result = 123;
    EXPECT_TRUE(dispatchClipboardUpdate(&result));
    EXPECT_EQ(result, 0);
    EXPECT_EQ(signalCount, 1);
    EXPECT_EQ(listener_->text(), expected);
    EXPECT_EQ(observed, expected);
}

TEST_F(WindowsCopyEventListenerIntegrationTest, ClipboardUpdateWithoutUnicodeTextClearsCachedText)
{
    listener_->lastText = QStringLiteral("stale");
    ASSERT_TRUE(clearClipboard());

    int signalCount = 0;
    QObject::connect(listener_, &AbstractCopyEventListener::clipboardChanged,
                     [&signalCount]() { ++signalCount; });

    EXPECT_TRUE(dispatchClipboardUpdate());
    EXPECT_EQ(signalCount, 1);
    EXPECT_TRUE(listener_->text().isEmpty());
}

TEST_F(WindowsCopyEventListenerIntegrationTest, NonClipboardMessageIsIgnored)
{
    listener_->lastText = QStringLiteral("unchanged");

    int signalCount = 0;
    QObject::connect(listener_, &AbstractCopyEventListener::clipboardChanged,
                     [&signalCount]() { ++signalCount; });

    qintptr result = 77;
    EXPECT_FALSE(dispatchMessage(WM_USER, &result));
    EXPECT_EQ(result, 77);
    EXPECT_EQ(signalCount, 0);
    EXPECT_EQ(listener_->text(), QStringLiteral("unchanged"));
}

#endif

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include "copy_event_listener_test.moc"
