#include "SQLService.hpp"

#include <QDebug>
#include <QRegularExpression>

namespace {

QString colorToString(const QColor& color) {
    return color.isValid() ? color.name(QColor::HexRgb) : QStringLiteral("#000000");
}

QColor colorFromColumn(sqlite3_stmt* stmt, int column, const QColor& fallback) {
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
    if (text == nullptr) {
        return fallback;
    }

    const QColor color(QString::fromUtf8(text));
    return color.isValid() ? color : fallback;
}

QDateTime dateTimeFromColumn(sqlite3_stmt* stmt, int column) {
    if (sqlite3_column_type(stmt, column) == SQLITE_NULL) {
        return {};
    }

    return QDateTime::fromSecsSinceEpoch(sqlite3_column_int64(stmt, column));
}

}  // namespace

bool SQLService::isReady() const {
    return db != nullptr;
}

bool SQLService::execute(const char* sql) {
    char* errMsg = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc == SQLITE_OK) {
        return true;
    }

    qWarning() << "sqlite exec failed:" << (errMsg != nullptr ? errMsg : sqlite3_errmsg(db));
    if (errMsg != nullptr) {
        sqlite3_free(errMsg);
    }
    return false;
}

bool SQLService::resetStatement(sqlite3_stmt* stmt) const {
    if (stmt == nullptr) {
        return false;
    }

    const int resetRc = sqlite3_reset(stmt);
    const int clearRc = sqlite3_clear_bindings(stmt);
    return resetRc == SQLITE_OK && clearRc == SQLITE_OK;
}

QString SQLService::lastError() const {
    if (db == nullptr) {
        return QStringLiteral("database is not initialized");
    }

    return QString::fromUtf8(sqlite3_errmsg(db));
}

ContentListItemData SQLService::makeContentItem(sqlite3_stmt* stmt) const {
    const auto* tagNameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const auto* ruleText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

    Tag tag(
        tagNameText != nullptr ? QString::fromUtf8(tagNameText) : QString(),
        ruleText != nullptr ? QString::fromUtf8(ruleText) : QString(),
        static_cast<SearchMode>(sqlite3_column_int(stmt, 5)),
        colorFromColumn(stmt, 3, QColor(255, 255, 255)),
        colorFromColumn(stmt, 2, QColor(0, 0, 0)),
        sqlite3_column_int(stmt, 4) != 0
    );

    const auto* contentText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    ContentListItemData item(
        tag,
        contentText != nullptr ? QString::fromUtf8(contentText) : QString(),
        dateTimeFromColumn(stmt, 7),
        dateTimeFromColumn(stmt, 8)
    );

    const void* hashBlob = sqlite3_column_blob(stmt, 9);
    const int hashSize = sqlite3_column_bytes(stmt, 9);
    if (hashBlob != nullptr && hashSize > 0) {
        item.hash = QByteArray(static_cast<const char*>(hashBlob), hashSize);
    }
    item.pinned = sqlite3_column_int(stmt, 10) != 0;
    return item;
}

QVector<ContentListItemData> SQLService::searchByTag(sqlite3_int64 tagId, const QString& str) {
    QVector<ContentListItemData> results;
    if (!isReady()) {
        return results;
    }

    QString sql =
        "SELECT t.tagName, t.rule, t.tagNameColor, t.tagBackColor, t.isSysTag, t.mode, "
        "c.content, c.copyTime, c.updateTime, c.hash, c.pinned "
        "FROM ContentItem c "
        "LEFT JOIN Tag t ON c.tag_id = t.id "
        "WHERE c.tag_id = ? ";

    if (!str.trimmed().isEmpty()) {
        sql += "AND c.content LIKE ? ";
    }

    sql += "ORDER BY c.pinned DESC, c.updateTime DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.toUtf8().constData(), -1, &stmt, nullptr) != SQLITE_OK) {
        qWarning() << "prepare searchByTag failed:" << lastError();
        return results;
    }

    int bindIndex = 1;
    sqlite3_bind_int64(stmt, bindIndex++, tagId);
    if (!str.trimmed().isEmpty()) {
        const QString pattern = QStringLiteral("%") + str + QStringLiteral("%");
        sqlite3_bind_text(stmt, bindIndex++, pattern.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(stmt, bindIndex, MAX_RESULT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(makeContentItem(stmt));
    }

    sqlite3_finalize(stmt);
    return results;
}

bool SQLService::clear(QDateTime time) {
    if (!isReady()) {
        return false;
    }

    const char* sql = "DELETE FROM ContentItem WHERE updateTime < ? AND pinned = 0;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        qWarning() << "prepare clear failed:" << lastError();
        return false;
    }

    sqlite3_bind_int64(stmt, 1, time.toSecsSinceEpoch());
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        qWarning() << "clear failed:" << lastError();
    }

    sqlite3_finalize(stmt);
    return ok;
}

sqlite3_int64 SQLService::searchTag(const QString& tagName) {
    if (!isReady() || searchTagStmt == nullptr) {
        return -2;
    }

    resetStatement(searchTagStmt);
    const QByteArray tagNameUtf8 = tagName.toUtf8();
    const int rc = sqlite3_bind_text(searchTagStmt, 1, tagNameUtf8.constData(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        qWarning() << "bind searchTag failed:" << lastError();
        resetStatement(searchTagStmt);
        return -2;
    }

    sqlite3_int64 tagId = -1;
    const int stepRc = sqlite3_step(searchTagStmt);
    if (stepRc == SQLITE_ROW) {
        tagId = sqlite3_column_int64(searchTagStmt, 0);
    } else if (stepRc != SQLITE_DONE) {
        qWarning() << "step searchTag failed:" << lastError();
        tagId = -2;
    }

    resetStatement(searchTagStmt);
    return tagId;
}

SQLService::SQLService(QObject* parent) : QObject(parent) {
    databaseDir = QDir(QDir::homePath() + DATABASE_DIR);
    if (!databaseDir.exists() && !QDir().mkpath(databaseDir.absolutePath())) {
        qWarning() << "创建数据库目录失败:" << databaseDir.absolutePath();
        return;
    }

    const QString dbPath = databaseDir.filePath(DATABASE_NAME);
    const int openRc = sqlite3_open(dbPath.toUtf8().constData(), &db);
    if (openRc != SQLITE_OK) {
        qWarning() << "打开数据库失败:" << dbPath << lastError();
        if (db != nullptr) {
            sqlite3_close(db);
            db = nullptr;
        }
        return;
    }

    if (!execute(TABLE_TAG) || !execute(TABLE_CONTENT)) {
        sqlite3_close(db);
        db = nullptr;
        return;
    }

    if (sqlite3_prepare_v2(db, tagSQL, -1, &tagStmt, nullptr) != SQLITE_OK
        || sqlite3_prepare_v2(db, contentSQL, -1, &contentStmt, nullptr) != SQLITE_OK
        || sqlite3_prepare_v2(db, sqlSearchTag, -1, &searchTagStmt, nullptr) != SQLITE_OK) {
        qWarning() << "prepare statement failed:" << lastError();
        sqlite3_finalize(tagStmt);
        sqlite3_finalize(contentStmt);
        sqlite3_finalize(searchTagStmt);
        tagStmt = nullptr;
        contentStmt = nullptr;
        searchTagStmt = nullptr;
        sqlite3_close(db);
        db = nullptr;
    }
}

SQLService::~SQLService() {
    sqlite3_finalize(tagStmt);
    sqlite3_finalize(contentStmt);
    sqlite3_finalize(searchTagStmt);
    sqlite3_close(db);
}

QString SQLService::save(const Tag& tag) {
    if (!isReady() || tagStmt == nullptr) {
        return QStringLiteral("database is not initialized");
    }

    const sqlite3_int64 existingTagId = searchTag(tag.tagName);
    if (existingTagId >= 0) {
        return {};
    }
    if (existingTagId == -2) {
        return lastError();
    }

    resetStatement(tagStmt);

    const QByteArray tagNameUtf8 = tag.tagName.toUtf8();
    const QByteArray ruleUtf8 = tag.rule.toUtf8();
    const QByteArray nameColorUtf8 = colorToString(tag.tagNameColor).toUtf8();
    const QByteArray backColorUtf8 = colorToString(tag.tagBackColor).toUtf8();

    sqlite3_bind_text(tagStmt, 1, tagNameUtf8.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(tagStmt, 2, ruleUtf8.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(tagStmt, 3, nameColorUtf8.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(tagStmt, 4, backColorUtf8.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(tagStmt, 5, tag.isSysTag ? 1 : 0);
    sqlite3_bind_int(tagStmt, 6, static_cast<int>(tag.mode));

    const int stepRc = sqlite3_step(tagStmt);
    if (stepRc != SQLITE_DONE) {
        const QString error = lastError();
        qWarning() << "save tag failed:" << error;
        resetStatement(tagStmt);
        return error;
    }

    resetStatement(tagStmt);
    return {};
}

QString SQLService::save(const ContentListItemData& data) {
    if (!isReady() || contentStmt == nullptr) {
        return QStringLiteral("database is not initialized");
    }

    QString error = save(data.tag);
    if (!error.isEmpty()) {
        return error;
    }

    const sqlite3_int64 tagId = searchTag(data.tag.tagName);
    if (tagId < 0) {
        return tagId == -1 ? QStringLiteral("tag not found after save") : lastError();
    }

    resetStatement(contentStmt);

    const QByteArray contentUtf8 = data.content.toUtf8();
    const QDateTime copyTime = data.copyTime.isValid() ? data.copyTime : QDateTime::currentDateTime();
    const QDateTime updateTime = data.updateTime.isValid() ? data.updateTime : copyTime;
    const QByteArray hash = data.hash.isEmpty()
        ? QCryptographicHash::hash(data.content.toUtf8(), QCryptographicHash::Sha256)
        : data.hash;

    sqlite3_bind_int64(contentStmt, 1, tagId);
    sqlite3_bind_text(contentStmt, 2, contentUtf8.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(contentStmt, 3, copyTime.toSecsSinceEpoch());
    sqlite3_bind_int64(contentStmt, 4, updateTime.toSecsSinceEpoch());
    sqlite3_bind_blob(contentStmt, 5, hash.constData(), hash.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(contentStmt, 6, data.pinned ? 1 : 0);

    const int stepRc = sqlite3_step(contentStmt);
    if (stepRc != SQLITE_DONE) {
        error = lastError();
        qWarning() << "save content failed:" << error;
    }

    resetStatement(contentStmt);
    return error;
}

QVector<ContentListItemData> SQLService::search(QString str, QString rule, SearchMode mode) {
    QVector<ContentListItemData> results;
    if (!isReady()) {
        return results;
    }

    const auto searchAllLike = [&]() {
        sqlite3_stmt* stmt = nullptr;
        const char* sql =
            "SELECT t.tagName, t.rule, t.tagNameColor, t.tagBackColor, t.isSysTag, t.mode, "
            "c.content, c.copyTime, c.updateTime, c.hash, c.pinned "
            "FROM ContentItem c "
            "LEFT JOIN Tag t ON c.tag_id = t.id "
            "WHERE c.content LIKE ? "
            "ORDER BY c.pinned DESC, c.updateTime DESC LIMIT ?;";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            qWarning() << "prepare search all failed:" << lastError();
            return QVector<ContentListItemData>{};
        }

        const QString pattern = QStringLiteral("%") + str + QStringLiteral("%");
        sqlite3_bind_text(stmt, 1, pattern.toUtf8().constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, MAX_RESULT);

        QVector<ContentListItemData> items;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            items.push_back(makeContentItem(stmt));
        }

        sqlite3_finalize(stmt);
        return items;
    };

    const QString trimmedStr = str.trimmed();
    const QString trimmedRule = rule.trimmed();
    if (trimmedStr.isEmpty()) {
        return results;
    }

    if (mode == SearchMode::Regex && !trimmedRule.isEmpty()) {
        const QRegularExpression regex(trimmedRule);
        if (regex.isValid()) {
            sqlite3_stmt* stmt = nullptr;
            const char* sql =
                "SELECT t.tagName, t.rule, t.tagNameColor, t.tagBackColor, t.isSysTag, t.mode, "
                "c.content, c.copyTime, c.updateTime, c.hash, c.pinned "
                "FROM ContentItem c "
                "LEFT JOIN Tag t ON c.tag_id = t.id "
                "ORDER BY c.pinned DESC, c.updateTime DESC;";

            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                qWarning() << "prepare regex search failed:" << lastError();
                return results;
            }

            while (sqlite3_step(stmt) == SQLITE_ROW && results.size() < MAX_RESULT) {
                ContentListItemData item = makeContentItem(stmt);
                if (regex.match(item.content).hasMatch()) {
                    results.push_back(std::move(item));
                }
            }

            sqlite3_finalize(stmt);
            return results;
        }
    }

    return searchAllLike();
}

QVector<ContentListItemData> SQLService::search(QString str, QString rule, Tag& tag, SearchMode mode) {
    Q_UNUSED(rule);
    Q_UNUSED(mode);

    const sqlite3_int64 tagId = searchTag(tag.tagName);
    if (tagId < 0) {
        return {};
    }

    return searchByTag(tagId, str);
}

bool SQLService::deleteItem(QByteArray hash) {
    if (!isReady()) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM ContentItem WHERE hash = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        qWarning() << "prepare deleteItem failed:" << lastError();
        return false;
    }

    sqlite3_bind_blob(stmt, 1, hash.constData(), hash.size(), SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        qWarning() << "deleteItem failed:" << lastError();
    }

    sqlite3_finalize(stmt);
    return ok;
}

QVector<ContentListItemData> SQLService::get() {
    QVector<ContentListItemData> results;
    if (!isReady()) {
        return results;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT t.tagName, t.rule, t.tagNameColor, t.tagBackColor, t.isSysTag, t.mode, "
        "c.content, c.copyTime, c.updateTime, c.hash, c.pinned "
        "FROM ContentItem c "
        "LEFT JOIN Tag t ON c.tag_id = t.id "
        "ORDER BY c.pinned DESC, c.updateTime DESC LIMIT ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        qWarning() << "prepare get failed:" << lastError();
        return results;
    }

    sqlite3_bind_int(stmt, 1, MAX_ITEM);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(makeContentItem(stmt));
    }

    sqlite3_finalize(stmt);
    return results;
}
