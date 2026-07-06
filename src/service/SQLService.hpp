#include <QObject>
#include <QDateTime>
#include <QVector>
#include <QDir>

#include "sqlite3.h"

#include "../struct.hpp"

#define DATABASE_DIR "/AppData/Local/ClipMind"
#define DATABASE_NAME "Clipboard.db"
#define TABLE_TAG \
"-- 创建 Tag 表\n" \
"CREATE TABLE IF NOT EXISTS Tag (\n" \
"    id INTEGER PRIMARY KEY AUTOINCREMENT,\n" \
"    tagName TEXT NOT NULL,\n" \
"    rule TEXT,\n" \
"    tagNameColor TEXT,      -- 存储为字符串\n" \
"    tagBackColor TEXT,\n" \
"    isSysTag INTEGER,       -- 0 或 1\n" \
"    mode INTEGER            -- SearchMode 枚举值\n" \
");"
#define TABLE_CONTENT \
"-- 创建 ContentItem 表\n" \
"CREATE TABLE IF NOT EXISTS ContentItem (\n" \
"    id INTEGER PRIMARY KEY AUTOINCREMENT,\n" \
"    tag_id INTEGER REFERENCES Tag(id) ON DELETE SET NULL, -- 删除标签时置空\n" \
"    content TEXT NOT NULL,\n" \
"    copyTime INTEGER,\n" \
"    updateTime INTEGER,     -- Unix 时间戳（秒），可排序\n" \
"    hash BLOB UNIQUE,       -- 内容唯一标识，设为 UNIQUE\n" \
"    pinned INTEGER DEFAULT 0\n" \
");"

class SQLService : public QObject {
    Q_OBJECT
private:
    bool clear(QDateTime time); // 把time以前的条目删除(数据库中对应的字段是updateTime)
    sqlite3_int64 searchTag(const QString& tagName);
    QDir databaseDir;

    sqlite3* db;

    sqlite3_stmt* tagStmt;
    const char* tagSQL = "INSERT INTO Tag (tagName, rule, tagNameColor, tagBackColor, isSysTag, mode) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* contentStmt;
    const char* contentSQL = "INSERT INTO ContentItem (tag_id, content, copyTime, updateTime, hash, pinned) VALUES (?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* searchTagStmt;
    const char* sqlSearchTag = "SELECT id FROM Tag WHERE tagName = ?;";

    int MAX_ITEM = 25;
    int MAX_RESULT = 100;

public:
    SQLService(QObject* parent = nullptr);
    ~SQLService();

    QString save(const ContentListItemData& data);
    QString save(const Tag& tag);
    QVector<ContentListItemData> search(QString str, QString rule, SearchMode mode = SearchMode::Regex);    // 根据提供的rule检索, 返回符合的对象(限制MAX_RESULT个)
    QVector<ContentListItemData> search(QString str, QString rule, Tag& tag, SearchMode mode = SearchMode::Regex);    // 根据提供的rule检索, 返回符合的对象(限制MAX_RESULT个), 根据tag进行一级搜索, 然后再用str二级搜索(若str为空就不用二级搜索)
    bool deleteItem(QByteArray hash);   // 后面再添加其他删除(比如正则表达式删除等)
    QVector<ContentListItemData> get(); // 根据updateTime倒序读取前MAX_ITEM个对象
};
