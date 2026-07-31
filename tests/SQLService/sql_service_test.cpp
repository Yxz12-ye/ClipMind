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

TEST(SQLServiceTest, TagCrudPersistsAcrossReload) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QDir dbDir(tempDir.filePath(QStringLiteral("db")));

    {
        SQLService service(dbDir);

        // 首次初始化写入 TEXT 和 LINK 系统保留标签
        QVector<Tag> tags = service.getTags();
        ASSERT_EQ(tags.size(), 2);
        EXPECT_EQ(tags[0].tagName, QStringLiteral("TEXT"));
        EXPECT_EQ(tags[1].tagName, QStringLiteral("LINK"));
        EXPECT_TRUE(tags[0].isSysTag);
        EXPECT_TRUE(tags[1].isSysTag);

        // 新增标签
        const Tag workTag{QStringLiteral("工作"), QStringLiteral("work"), SearchMode::Regex,
                          QColor("#DBEAFE"), QColor("#1D4ED8")};
        EXPECT_TRUE(service.save(workTag).isEmpty());

        // 重命名成功, 重名为已有标签时失败
        const Tag renamed{QStringLiteral("项目"), QStringLiteral("project|任务"), SearchMode::Regex,
                          QColor("#FEF3C7"), QColor("#B45309")};
        EXPECT_TRUE(service.updateTag(QStringLiteral("工作"), renamed));
        EXPECT_FALSE(service.updateTag(QStringLiteral("项目"),
                                       Tag{QStringLiteral("LINK"), QString(), SearchMode::None}));

        // 排序
        const Tag imageTag{QStringLiteral("图片"), QString(), SearchMode::None};
        EXPECT_TRUE(service.save(imageTag).isEmpty());
        EXPECT_TRUE(service.reorderTags({QStringLiteral("图片"), QStringLiteral("项目"),
                                         QStringLiteral("TEXT"), QStringLiteral("LINK")}));
    }

    {
        // 重新打开数据库验证持久化
        SQLService service(dbDir);
        QVector<Tag> tags = service.getTags();
        ASSERT_EQ(tags.size(), 4);
        EXPECT_EQ(tags[0].tagName, QStringLiteral("图片"));
        EXPECT_EQ(tags[1].tagName, QStringLiteral("项目"));
        EXPECT_EQ(tags[2].tagName, QStringLiteral("TEXT"));
        EXPECT_EQ(tags[3].tagName, QStringLiteral("LINK"));
        EXPECT_EQ(tags[1].rule, QStringLiteral("project|任务"));
        EXPECT_EQ(tags[1].mode, SearchMode::Regex);
        EXPECT_EQ(tags[1].tagBackColor, QColor("#FEF3C7"));
        EXPECT_EQ(tags[1].tagNameColor, QColor("#B45309"));

        // 删除
        EXPECT_TRUE(service.deleteTag(QStringLiteral("图片")));
        tags = service.getTags();
        ASSERT_EQ(tags.size(), 3);
        EXPECT_EQ(tags[0].tagName, QStringLiteral("项目"));
    }
}

TEST(SQLServiceTest, ExistingDatabasePromotesTextAndAddsMissingLinkSystemTag) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QDir dbDir(tempDir.filePath(QStringLiteral("db")));
    ASSERT_TRUE(QDir().mkpath(dbDir.absolutePath()));

    sqlite3* legacyDb = nullptr;
    const QByteArray dbPath = dbDir.filePath(DATABASE_NAME).toUtf8();
    ASSERT_EQ(sqlite3_open(dbPath.constData(), &legacyDb), SQLITE_OK);

    const char* legacySchema =
        "CREATE TABLE Tag ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "tagName TEXT NOT NULL,"
        "rule TEXT,"
        "tagNameColor TEXT,"
        "tagBackColor TEXT,"
        "isSysTag INTEGER,"
        "mode INTEGER);"
        "INSERT INTO Tag (tagName, rule, tagNameColor, tagBackColor, isSysTag, mode) "
        "VALUES ('TEXT', '', '#000000', '#FFFFFF', 0, 2);";
    char* error = nullptr;
    const int execResult = sqlite3_exec(legacyDb, legacySchema, nullptr, nullptr, &error);
    if (error != nullptr) {
        sqlite3_free(error);
    }
    EXPECT_EQ(execResult, SQLITE_OK);
    ASSERT_EQ(sqlite3_close(legacyDb), SQLITE_OK);

    SQLService service(dbDir);
    const QVector<Tag> tags = service.getTags();
    ASSERT_EQ(tags.size(), 2);
    EXPECT_EQ(tags[0].tagName, QStringLiteral("TEXT"));
    EXPECT_TRUE(tags[0].isSysTag);
    EXPECT_EQ(tags[1].tagName, QStringLiteral("LINK"));
    EXPECT_TRUE(tags[1].isSysTag);

    EXPECT_FALSE(service.deleteTag(QStringLiteral("TEXT")));
    EXPECT_FALSE(service.updateTag(
        QStringLiteral("LINK"),
        Tag{QStringLiteral("URL"), QString(), SearchMode::None, QColor(), QColor()}));
}

TEST(SQLServiceTest, DeletingTagClearsContentTagReference) {
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    SQLService service(QDir(tempDir.filePath(QStringLiteral("db"))));

    const QDateTime time = QDateTime::fromSecsSinceEpoch(100);
    const Tag imageTag{QStringLiteral("图片"), QString(), SearchMode::None};
    ASSERT_TRUE(
        service.save(ContentListItemData{imageTag, QStringLiteral("image content"), time, time})
            .isEmpty());

    ASSERT_TRUE(service.deleteTag(QStringLiteral("图片")));

    const QVector<ContentListItemData> items = service.get();
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items.front().content, QStringLiteral("image content"));
    EXPECT_TRUE(items.front().tag.tagName.isEmpty());  // 删除标签后关联内容的外键被置空
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
