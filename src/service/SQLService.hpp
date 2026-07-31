#pragma once

#include <QDateTime>
#include <QDir>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "../struct.hpp"
#include "sqlite3.h"

#define DATABASE_DIR "/AppData/Local/ClipMind"
#define DATABASE_NAME "Clipboard.db"
#define TABLE_TAG                                              \
    "-- 创建 Tag 表\n"                                         \
    "CREATE TABLE IF NOT EXISTS Tag (\n"                       \
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"              \
    "    tagName TEXT NOT NULL,\n"                             \
    "    rule TEXT,\n"                                         \
    "    tagNameColor TEXT,      -- 存储为字符串\n"            \
    "    tagBackColor TEXT,\n"                                 \
    "    isSysTag INTEGER,       -- 0 或 1\n"                  \
    "    mode INTEGER,           -- SearchMode 枚举值\n"       \
    "    priority INTEGER DEFAULT 0 -- 显示顺序, 越小越靠前\n" \
    ");"
#define TABLE_CONTENT                                                               \
    "-- 创建 ContentItem 表\n"                                                      \
    "CREATE TABLE IF NOT EXISTS ContentItem (\n"                                    \
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"                                   \
    "    tag_id INTEGER REFERENCES Tag(id) ON DELETE SET NULL, -- 删除标签时置空\n" \
    "    content TEXT NOT NULL,\n"                                                  \
    "    copyTime INTEGER,\n"                                                       \
    "    updateTime INTEGER,     -- Unix 时间戳（秒），可排序\n"                    \
    "    hash BLOB UNIQUE,       -- 内容唯一标识，设为 UNIQUE\n"                    \
    "    pinned INTEGER DEFAULT 0\n"                                                \
    ");"

class SQLService : public QObject {
    Q_OBJECT
private:
    bool isReady() const;
    bool execute(const char* sql);
    bool resetStatement(sqlite3_stmt* stmt) const;
    QString lastError() const;
    QVector<ContentListItemData> searchByTag(sqlite3_int64 tagId, const QString& str);
    ContentListItemData makeContentItem(sqlite3_stmt* stmt) const;

    bool clear(QDateTime time);  // 把time以前的条目删除(数据库中对应的字段是updateTime)
    sqlite3_int64 searchTag(const QString& tagName);
    void ensurePriorityColumn();  // 旧库迁移: 补充 priority 列
    void ensureSystemTags();      // 补齐 TEXT/LINK 系统保留标签
    QDir databaseDir;

    sqlite3* db = nullptr;

    sqlite3_stmt* tagStmt = nullptr;
    const char* tagSQL =
        "INSERT INTO Tag (tagName, rule, tagNameColor, tagBackColor, isSysTag, mode, priority) "
        "VALUES (?, ?, ?, ?, ?, ?, (SELECT COALESCE(MAX(priority), -1) + 1 FROM Tag));";
    sqlite3_stmt* contentStmt = nullptr;
    const char* contentSQL =
        "INSERT INTO ContentItem (tag_id, content, copyTime, updateTime, hash, pinned) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(hash) DO UPDATE SET "
        "updateTime = excluded.updateTime;";

    sqlite3_stmt* searchTagStmt = nullptr;
    const char* sqlSearchTag = "SELECT id FROM Tag WHERE tagName = ?;";

    const int MAX_ITEM = 25;
    const int MAX_RESULT = 100;

public:
    SQLService(QObject* parent = nullptr);
    SQLService(const QDir& databaseDirectory, QObject* parent = nullptr);
    ~SQLService();

    QString save(const ContentListItemData& data);
    QString save(const Tag& tag);
    QVector<ContentListItemData> search(
        QString rule,
        SearchMode mode =
            SearchMode::None);  // 根据提供的rule检索, 返回符合的对象(限制MAX_RESULT个)
    QVector<ContentListItemData> search(
        QString str, QString rule, Tag& tag,
        SearchMode mode =
            SearchMode::Regex);  // 根据提供的rule检索, 返回符合的对象(限制MAX_RESULT个),
                                 // 根据tag进行一级搜索,
                                 // 然后再用str二级搜索(若str为空就不用二级搜索)
    bool updateContentTime(const QString& content);
    bool deleteItem(QByteArray hash);    // 后面再添加其他删除(比如正则表达式删除等)
    QVector<ContentListItemData> get();  // 根据updateTime倒序读取前MAX_ITEM个对象

    // 标签管理: 增删改查与显示排序
    QVector<Tag> getTags() const;  // 全部标签, 包含 TEXT/LINK 系统保留标签
    bool updateTag(const QString& originalName, const Tag& tag);  // 重名时返回 false
    bool deleteTag(const QString& tagName);
    bool reorderTags(const QStringList& tagNames);  // 按给定顺序写入 0..n-1
};
