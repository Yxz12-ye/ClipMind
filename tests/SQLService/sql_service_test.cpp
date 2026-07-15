#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "service/SQLService.hpp"

TEST(SQLServiceTest, SavingDuplicateContentRefreshesExistingUpdateTime) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    SQLService service(QDir(tempDir.filePath(QStringLiteral("db"))));

    const QDateTime firstTime = QDateTime::fromSecsSinceEpoch(100);
    const QDateTime secondTime = QDateTime::fromSecsSinceEpoch(200);
    const Tag textTag{QStringLiteral("TEXT"), QString(), SearchMode::None};

    ASSERT_TRUE(
        service
            .save(ContentListItemData{textTag, QStringLiteral("same text"), firstTime, firstTime})
            .isEmpty());
    ASSERT_TRUE(
        service
            .save(ContentListItemData{textTag, QStringLiteral("same text"), secondTime, secondTime})
            .isEmpty());

    const QVector<ContentListItemData> items = service.get();
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items.front().content, QStringLiteral("same text"));
    EXPECT_EQ(items.front().copyTime, firstTime);
    EXPECT_EQ(items.front().updateTime, secondTime);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
