#include "SQLService.hpp"

#include <QRegularExpression>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

#include "LogService.hpp"

namespace {

using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
static_assert(sizeof(float) == 4, "sqlite-vec requires float32 embeddings");

QString validateEmbedding(const QVector<float>& embedding, const QString& model) {
    if (model.trimmed().isEmpty()) {
        return QStringLiteral("embedding model must not be empty");
    }
    if (embedding.isEmpty() ||
        embedding.size() > std::numeric_limits<int>::max() / static_cast<int>(sizeof(float))) {
        return QStringLiteral("invalid embedding dimension");
    }
    double normSquared = 0.0;
    for (float value : embedding) {
        if (!std::isfinite(value)) {
            return QStringLiteral("embedding values must be finite");
        }
        normSquared += static_cast<double>(value) * value;
    }
    if (normSquared < std::numeric_limits<float>::min() ||
        normSquared > std::numeric_limits<float>::max()) {
        return QStringLiteral("embedding squared norm must be in the normal float32 range");
    }
    return {};
}

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

    LogService::warn("SQLService", "sqlite exec failed: {}",
                     errMsg != nullptr ? errMsg : sqlite3_errmsg(db));
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

    Tag tag(tagNameText != nullptr ? QString::fromUtf8(tagNameText) : QString(),
            ruleText != nullptr ? QString::fromUtf8(ruleText) : QString(),
            static_cast<SearchMode>(sqlite3_column_int(stmt, 5)),
            colorFromColumn(stmt, 3, QColor(255, 255, 255)),
            colorFromColumn(stmt, 2, QColor(0, 0, 0)), sqlite3_column_int(stmt, 4) != 0);

    const auto* contentText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    ContentListItemData item(tag,
                             contentText != nullptr ? QString::fromUtf8(contentText) : QString(),
                             dateTimeFromColumn(stmt, 7), dateTimeFromColumn(stmt, 8));

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
        LogService::warn("SQLService", "prepare searchByTag failed: {}", lastError());
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
        LogService::warn("SQLService", "prepare clear failed: {}", lastError());
        return false;
    }

    sqlite3_bind_int64(stmt, 1, time.toSecsSinceEpoch());
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        LogService::warn("SQLService", "clear failed: {}", lastError());
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
    const int rc =
        sqlite3_bind_text(searchTagStmt, 1, tagNameUtf8.constData(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        LogService::warn("SQLService", "bind searchTag failed: {}", lastError());
        resetStatement(searchTagStmt);
        return -2;
    }

    sqlite3_int64 tagId = -1;
    const int stepRc = sqlite3_step(searchTagStmt);
    if (stepRc == SQLITE_ROW) {
        tagId = sqlite3_column_int64(searchTagStmt, 0);
    } else if (stepRc != SQLITE_DONE) {
        LogService::warn("SQLService", "step searchTag failed: {}", lastError());
        tagId = -2;
    }

    resetStatement(searchTagStmt);
    return tagId;
}

void SQLService::ensurePriorityColumn() {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "PRAGMA table_info(Tag);", -1, &stmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare table_info failed: {}", lastError());
        return;
    }

    bool hasPriority = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* columnName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (columnName != nullptr && QString::fromUtf8(columnName) == QLatin1String("priority")) {
            hasPriority = true;
            break;
        }
    }
    sqlite3_finalize(stmt);

    if (!hasPriority && !execute("ALTER TABLE Tag ADD COLUMN priority INTEGER DEFAULT 0;")) {
        LogService::warn("SQLService", "failed to add priority column");
    }
}

void SQLService::ensureSystemTags() {
    if (!isReady() || tagStmt == nullptr) {
        return;
    }

    const QVector<Tag> systemTags = {
        Tag(QStringLiteral("TEXT"), QString(), SearchMode::None, QColor("#FFFFFF"),
            QColor("#000000"), true),
        Tag(QStringLiteral("LINK"), QStringLiteral("https?://\\S+"), SearchMode::Regex,
            QColor("#DBEAFE"), QColor("#1D4ED8"), true),
    };

    for (const Tag& tag : systemTags) {
        const QString error = save(tag);
        if (!error.isEmpty()) {
            LogService::warn("SQLService", "ensure system tag failed: {} {}", tag.tagName, error);
        }
    }

    // 旧版在首次复制时会隐式创建非系统 TEXT 标签, 升级后需将其标记为保留标签。
    if (!execute("UPDATE Tag SET isSysTag = 1 WHERE tagName IN ('TEXT', 'LINK');")) {
        LogService::warn("SQLService", "mark system tags failed");
    }
}

SQLService::SQLService(QObject* parent)
    : SQLService(QDir(QDir::homePath() + DATABASE_DIR), parent) {}

SQLService::SQLService(const QDir& databaseDirectory, QObject* parent)
    : QObject(parent), databaseDir(databaseDirectory) {
    LogService::info("SQLService", "database location: {}", databaseDir.absolutePath());
    if (!databaseDir.exists() && !QDir().mkpath(databaseDir.absolutePath())) {
        LogService::warn("SQLService", "failed to create database directory: {}",
                         databaseDir.absolutePath());
        return;
    }

    const QString dbPath = databaseDir.filePath(DATABASE_NAME);
    const int openRc = sqlite3_open(dbPath.toUtf8().constData(), &db);
    if (openRc != SQLITE_OK) {
        LogService::warn("SQLService", "failed to open database: {} {}", dbPath, lastError());
        if (db != nullptr) {
            sqlite3_close(db);
            db = nullptr;
        }
        return;
    }

    char* vecError = nullptr;
    const int vecRc = sqlite3_vec_init(db, &vecError, nullptr);
    if (vecRc != SQLITE_OK) {
        LogService::warn("SQLService", "initialize sqlite-vec failed: {}",
                         vecError != nullptr ? vecError : sqlite3_errmsg(db));
    }
    sqlite3_free(vecError);

    if (vecRc != SQLITE_OK || !execute(TABLE_TAG) || !execute(TABLE_CONTENT) ||
        !execute("PRAGMA foreign_keys = ON;") ||
        !execute("CREATE TABLE IF NOT EXISTS ContentEmbedding ("
                 "hash BLOB NOT NULL REFERENCES ContentItem(hash) ON DELETE CASCADE, "
                 "model TEXT NOT NULL, dimensions INTEGER NOT NULL CHECK(dimensions > 0), "
                 "embedding BLOB NOT NULL CHECK(typeof(embedding) = 'blob' "
                 "AND length(embedding) = dimensions * 4), PRIMARY KEY(hash, model));"
                 "CREATE INDEX IF NOT EXISTS ContentEmbedding_model_dimensions "
                 "ON ContentEmbedding(model, dimensions);")) {
        sqlite3_close(db);
        db = nullptr;
        return;
    }

    ensurePriorityColumn();  // 兼容旧数据库(缺少 priority 列)

    if (sqlite3_prepare_v2(db, tagSQL, -1, &tagStmt, nullptr) != SQLITE_OK ||
        sqlite3_prepare_v2(db, contentSQL, -1, &contentStmt, nullptr) != SQLITE_OK ||
        sqlite3_prepare_v2(db, sqlSearchTag, -1, &searchTagStmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare statement failed: {}", lastError());
        sqlite3_finalize(tagStmt);
        sqlite3_finalize(contentStmt);
        sqlite3_finalize(searchTagStmt);
        tagStmt = nullptr;
        contentStmt = nullptr;
        searchTagStmt = nullptr;
        sqlite3_close(db);
        db = nullptr;
        return;
    }

    ensureSystemTags();
}

SQLService::~SQLService() {
    sqlite3_finalize(tagStmt);
    sqlite3_finalize(contentStmt);
    sqlite3_finalize(searchTagStmt);
    sqlite3_close(db);
}

QString SQLService::saveEmbedding(const QByteArray& hash, const QString& model,
                                  const QVector<float>& embedding) {
    if (!isReady()) {
        return lastError();
    }
    const QString validationError = validateEmbedding(embedding, model);
    if (!validationError.isEmpty()) {
        return validationError;
    }
    if (hash.isEmpty()) {
        return QStringLiteral("content hash must not be empty");
    }

    sqlite3_stmt* raw = nullptr;
    const int prepareRc = sqlite3_prepare_v2(
        db,
        "INSERT INTO ContentEmbedding(hash, model, dimensions, embedding) VALUES (?, ?, ?, ?) "
        "ON CONFLICT(hash, model) DO UPDATE SET dimensions = excluded.dimensions, "
        "embedding = excluded.embedding;",
        -1, &raw, nullptr);
    Statement stmt(raw, sqlite3_finalize);
    if (prepareRc != SQLITE_OK) {
        return lastError();
    }
    const QByteArray modelUtf8 = model.toUtf8();
    if (sqlite3_bind_blob64(raw, 1, hash.constData(), hash.size(), SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text64(raw, 2, modelUtf8.constData(), modelUtf8.size(), SQLITE_TRANSIENT,
                            SQLITE_UTF8) != SQLITE_OK ||
        sqlite3_bind_int64(raw, 3, embedding.size()) != SQLITE_OK ||
        sqlite3_bind_blob64(raw, 4, embedding.constData(), embedding.size() * sizeof(float),
                            SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(raw) != SQLITE_DONE) {
        return lastError();
    }
    return {};
}

QVector<float> SQLService::getEmbedding(const QByteArray& hash, const QString& model,
                                        QString* error) const {
    QString localError;
    if (error == nullptr) {
        error = &localError;
    }
    error->clear();
    if (!isReady()) {
        *error = lastError();
        return {};
    }
    sqlite3_stmt* raw = nullptr;
    const int prepareRc = sqlite3_prepare_v2(
        db, "SELECT embedding FROM ContentEmbedding WHERE hash = ? AND model = ?;", -1, &raw,
        nullptr);
    Statement stmt(raw, sqlite3_finalize);
    const QByteArray modelUtf8 = model.toUtf8();
    if (prepareRc != SQLITE_OK ||
        sqlite3_bind_blob64(raw, 1, hash.constData(), hash.size(), SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text64(raw, 2, modelUtf8.constData(), modelUtf8.size(), SQLITE_TRANSIENT,
                            SQLITE_UTF8) != SQLITE_OK) {
        *error = lastError();
        return {};
    }
    const int rc = sqlite3_step(raw);
    if (rc == SQLITE_DONE) {
        return {};
    }
    if (rc != SQLITE_ROW) {
        *error = lastError();
        return {};
    }
    const int bytes = sqlite3_column_bytes(raw, 0);
    const void* blob = sqlite3_column_blob(raw, 0);
    if (blob == nullptr || bytes <= 0 || bytes % static_cast<int>(sizeof(float)) != 0) {
        *error = QStringLiteral("invalid stored embedding");
        return {};
    }
    QVector<float> result(bytes / static_cast<int>(sizeof(float)));
    std::memcpy(result.data(), blob, bytes);
    return result;
}

bool SQLService::deleteEmbedding(const QByteArray& hash, const QString& model) {
    if (!isReady()) {
        return false;
    }
    sqlite3_stmt* raw = nullptr;
    const int prepareRc = sqlite3_prepare_v2(
        db, "DELETE FROM ContentEmbedding WHERE hash = ? AND model = ?;", -1, &raw, nullptr);
    Statement stmt(raw, sqlite3_finalize);
    const QByteArray modelUtf8 = model.toUtf8();
    return prepareRc == SQLITE_OK &&
           sqlite3_bind_blob64(raw, 1, hash.constData(), hash.size(), SQLITE_TRANSIENT) ==
               SQLITE_OK &&
           sqlite3_bind_text64(raw, 2, modelUtf8.constData(), modelUtf8.size(), SQLITE_TRANSIENT,
                               SQLITE_UTF8) == SQLITE_OK &&
           sqlite3_step(raw) == SQLITE_DONE;
}

QVector<VectorSearchResult> SQLService::searchByEmbedding(const QVector<float>& embedding,
                                                          const QString& model, int limit,
                                                          QString* error) const {
    QString localError;
    if (error == nullptr) {
        error = &localError;
    }
    *error = isReady() ? validateEmbedding(embedding, model) : lastError();
    if (!error->isEmpty()) {
        return {};
    }
    if (limit < 1 || limit > MAX_RESULT) {
        *error = QStringLiteral("search limit must be between 1 and %1").arg(MAX_RESULT);
        return {};
    }

    // Scalar distance search supports different embedding dimensions without rebuilding vec0
    // tables.
    const char* sql =
        "SELECT t.tagName, t.rule, t.tagNameColor, t.tagBackColor, t.isSysTag, t.mode, "
        "c.content, c.copyTime, c.updateTime, c.hash, c.pinned, "
        "vec_distance_cosine(e.embedding, ?) AS distance "
        "FROM ContentEmbedding e JOIN ContentItem c ON c.hash = e.hash "
        "LEFT JOIN Tag t ON c.tag_id = t.id "
        "WHERE e.model = ? AND e.dimensions = ? ORDER BY distance ASC, c.id ASC LIMIT ?;";
    sqlite3_stmt* raw = nullptr;
    const int prepareRc = sqlite3_prepare_v2(db, sql, -1, &raw, nullptr);
    Statement stmt(raw, sqlite3_finalize);
    const QByteArray modelUtf8 = model.toUtf8();
    if (prepareRc != SQLITE_OK ||
        sqlite3_bind_blob64(raw, 1, embedding.constData(), embedding.size() * sizeof(float),
                            SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text64(raw, 2, modelUtf8.constData(), modelUtf8.size(), SQLITE_TRANSIENT,
                            SQLITE_UTF8) != SQLITE_OK ||
        sqlite3_bind_int64(raw, 3, embedding.size()) != SQLITE_OK ||
        sqlite3_bind_int(raw, 4, limit) != SQLITE_OK) {
        *error = lastError();
        return {};
    }
    QVector<VectorSearchResult> results;
    int rc;
    while ((rc = sqlite3_step(raw)) == SQLITE_ROW) {
        const double distance = sqlite3_column_double(raw, 11);
        if (sqlite3_column_type(raw, 11) == SQLITE_NULL || !std::isfinite(distance)) {
            *error = QStringLiteral("invalid cosine distance");
            return {};
        }
        results.push_back({makeContentItem(raw), distance});
    }
    if (rc != SQLITE_DONE) {
        *error = lastError();
        return {};
    }
    return results;
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
        LogService::warn("SQLService", "save tag failed: {}", error);
        resetStatement(tagStmt);
        return error;
    }

    resetStatement(tagStmt);
    return {};
}

QVector<Tag> SQLService::getTags() const {
    QVector<Tag> tags;
    if (!isReady()) {
        return tags;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT tagName, rule, tagNameColor, tagBackColor, isSysTag, mode, priority "
        "FROM Tag ORDER BY priority ASC, id ASC;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare getTags failed: {}", lastError());
        return tags;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const auto* rule = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        Tag tag(name != nullptr ? QString::fromUtf8(name) : QString(),
                rule != nullptr ? QString::fromUtf8(rule) : QString(),
                static_cast<SearchMode>(sqlite3_column_int(stmt, 5)),
                colorFromColumn(stmt, 3, QColor(255, 255, 255)),
                colorFromColumn(stmt, 2, QColor(0, 0, 0)), sqlite3_column_int(stmt, 4) != 0,
                sqlite3_column_int(stmt, 6));

        tags.push_back(std::move(tag));
    }

    sqlite3_finalize(stmt);
    return tags;
}

Tag SQLService::matchTag(const QString& content) const {
    Tag fallback(QStringLiteral("TEXT"), QString(), SearchMode::None, QColor("#FFFFFF"),
                 QColor("#000000"), true);

    const QVector<Tag> tags = getTags();
    for (const Tag& tag : tags) {
        if (tag.tagName == QLatin1String("TEXT")) {
            fallback = tag;
            continue;
        }

        // Semantics 暂不实现, None 不参与自动匹配。
        if (tag.mode != SearchMode::Regex || tag.rule.isEmpty()) {
            continue;
        }

        const QRegularExpression regex(tag.rule);
        if (!regex.isValid()) {
            LogService::warn("SQLService", "invalid tag regex: {} {}", tag.tagName,
                             regex.errorString());
            continue;
        }

        if (regex.match(content).hasMatch()) {
            return tag;
        }
    }

    return fallback;
}

bool SQLService::updateTag(const QString& originalName, const Tag& tag) {
    if (!isReady()) {
        return false;
    }

    if (tag.tagName != originalName) {
        const sqlite3_int64 existingId = searchTag(tag.tagName);
        if (existingId != -1) {
            return false;  // 重名或查询失败
        }
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "UPDATE Tag SET tagName = ?, rule = ?, tagNameColor = ?, tagBackColor = ?, isSysTag = ?, "
        "mode = ? WHERE tagName = ? AND isSysTag = 0;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare updateTag failed: {}", lastError());
        return false;
    }

    const QByteArray nameUtf8 = tag.tagName.toUtf8();
    const QByteArray ruleUtf8 = tag.rule.toUtf8();
    const QByteArray nameColorUtf8 = colorToString(tag.tagNameColor).toUtf8();
    const QByteArray backColorUtf8 = colorToString(tag.tagBackColor).toUtf8();
    const QByteArray originalNameUtf8 = originalName.toUtf8();

    sqlite3_bind_text(stmt, 1, nameUtf8.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, ruleUtf8.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, nameColorUtf8.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, backColorUtf8.constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, tag.isSysTag ? 1 : 0);
    sqlite3_bind_int(stmt, 6, static_cast<int>(tag.mode));
    sqlite3_bind_text(stmt, 7, originalNameUtf8.constData(), -1, SQLITE_TRANSIENT);

    const int stepRc = sqlite3_step(stmt);
    const bool ok = stepRc == SQLITE_DONE && sqlite3_changes(db) == 1;
    if (stepRc != SQLITE_DONE) {
        LogService::warn("SQLService", "updateTag failed: {}", lastError());
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool SQLService::deleteTag(const QString& tagName) {
    if (!isReady()) {
        return false;
    }

    const sqlite3_int64 textTagId = searchTag(QStringLiteral("TEXT"));
    if (textTagId < 0 || !execute("BEGIN TRANSACTION;")) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* reassignSql =
        "UPDATE ContentItem SET tag_id = ? "
        "WHERE tag_id = (SELECT id FROM Tag WHERE tagName = ? AND isSysTag = 0);";
    if (sqlite3_prepare_v2(db, reassignSql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare reassign deleted tag failed: {}", lastError());
        execute("ROLLBACK;");
        return false;
    }

    const QByteArray tagNameUtf8 = tagName.toUtf8();
    sqlite3_bind_int64(stmt, 1, textTagId);
    sqlite3_bind_text(stmt, 2, tagNameUtf8.constData(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        LogService::warn("SQLService", "reassign deleted tag failed: {}", lastError());
        sqlite3_finalize(stmt);
        execute("ROLLBACK;");
        return false;
    }
    sqlite3_finalize(stmt);

    const char* deleteSql = "DELETE FROM Tag WHERE tagName = ? AND isSysTag = 0;";
    if (sqlite3_prepare_v2(db, deleteSql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare deleteTag failed: {}", lastError());
        execute("ROLLBACK;");
        return false;
    }

    sqlite3_bind_text(stmt, 1, tagNameUtf8.constData(), -1, SQLITE_TRANSIENT);
    const int stepRc = sqlite3_step(stmt);
    const bool ok = stepRc == SQLITE_DONE && sqlite3_changes(db) == 1;
    if (stepRc != SQLITE_DONE) {
        LogService::warn("SQLService", "deleteTag failed: {}", lastError());
    }

    sqlite3_finalize(stmt);
    if (!ok) {
        execute("ROLLBACK;");
        return false;
    }

    if (!execute("COMMIT;")) {
        execute("ROLLBACK;");
        return false;
    }
    return true;
}

bool SQLService::reorderTags(const QStringList& tagNames) {
    if (!isReady()) {
        return false;
    }

    if (!execute("BEGIN TRANSACTION;")) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE Tag SET priority = ? WHERE tagName = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare reorderTags failed: {}", lastError());
        execute("ROLLBACK;");
        return false;
    }

    bool ok = true;
    for (int i = 0; i < tagNames.size(); ++i) {
        resetStatement(stmt);
        sqlite3_bind_int(stmt, 1, i);
        const QByteArray nameUtf8 = tagNames.at(i).toUtf8();
        sqlite3_bind_text(stmt, 2, nameUtf8.constData(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            LogService::warn("SQLService", "reorderTags step failed: {}", lastError());
            ok = false;
            break;
        }
    }

    sqlite3_finalize(stmt);
    if (!ok) {
        execute("ROLLBACK;");
        return false;
    }

    if (!execute("COMMIT;")) {
        execute("ROLLBACK;");
        return false;
    }
    return true;
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
    const QDateTime copyTime =
        data.copyTime.isValid() ? data.copyTime : QDateTime::currentDateTime();
    const QDateTime updateTime = data.updateTime.isValid() ? data.updateTime : copyTime;
    const QByteArray hash =
        data.hash.isEmpty()
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
        LogService::warn("SQLService", "save content failed: {}", error);
    }

    resetStatement(contentStmt);
    return error;
}

QVector<ContentListItemData> SQLService::search(QString rule, SearchMode mode) {
    QVector<ContentListItemData> results;
    if (!isReady()) {
        return results;
    }

    // 弃用
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
            LogService::warn("SQLService", "prepare search all failed: {}", lastError());
            return QVector<ContentListItemData>{};
        }

        const QString pattern = QStringLiteral("%") + rule + QStringLiteral("%");
        sqlite3_bind_text(stmt, 1, pattern.toUtf8().constData(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, MAX_RESULT);

        QVector<ContentListItemData> items;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            items.push_back(makeContentItem(stmt));
        }

        sqlite3_finalize(stmt);
        return items;
    };

    const QString trimmedRule = rule.trimmed();
    if (trimmedRule.isEmpty()) {
        return this->get();
    }

    if (mode == SearchMode::Regex) {
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
                LogService::warn("SQLService", "prepare regex search failed: {}", lastError());
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
        } else {
            LogService::warn("SQLService", "invalid regular expression!");
            return results;
        }
    }

    if (mode == SearchMode::None) {
        return searchAllLike();
    }

    return searchAllLike();
}

QVector<ContentListItemData> SQLService::search(QString str, QString rule, Tag& tag,
                                                SearchMode mode) {
    Q_UNUSED(rule);
    Q_UNUSED(mode);

    const sqlite3_int64 tagId = searchTag(tag.tagName);
    if (tagId < 0) {
        return {};
    }

    return searchByTag(tagId, str);
}

bool SQLService::updateContentTime(const QString& content) {
    if (!isReady()) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE ContentItem SET updateTime = ? WHERE content = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare update content time failed: {}", lastError());
        return false;
    }

    const QByteArray contentUtf8 = content.toUtf8();
    sqlite3_bind_int64(stmt, 1, QDateTime::currentDateTime().toSecsSinceEpoch());
    sqlite3_bind_text(stmt, 2, contentUtf8.constData(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        LogService::warn("SQLService", "update content time failed: {}", lastError());
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool SQLService::setPinned(const QByteArray& hash, bool pinned) {
    if (!isReady() || hash.isEmpty()) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE ContentItem SET pinned = ? WHERE hash = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare setPinned failed: {}", lastError());
        return false;
    }

    sqlite3_bind_int(stmt, 1, pinned ? 1 : 0);
    sqlite3_bind_blob(stmt, 2, hash.constData(), hash.size(), SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        LogService::warn("SQLService", "setPinned failed: {}", lastError());
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool SQLService::updateItemTag(const QByteArray& hash, const QString& tagName) {
    if (!isReady() || hash.isEmpty()) {
        return false;
    }

    const sqlite3_int64 tagId = searchTag(tagName);
    if (tagId < 0) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE ContentItem SET tag_id = ? WHERE hash = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare updateItemTag failed: {}", lastError());
        return false;
    }

    sqlite3_bind_int64(stmt, 1, tagId);
    sqlite3_bind_blob(stmt, 2, hash.constData(), hash.size(), SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        LogService::warn("SQLService", "updateItemTag failed: {}", lastError());
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool SQLService::deleteItem(QByteArray hash) {
    if (!isReady()) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM ContentItem WHERE hash = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LogService::warn("SQLService", "prepare deleteItem failed: {}", lastError());
        return false;
    }

    sqlite3_bind_blob(stmt, 1, hash.constData(), hash.size(), SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        LogService::warn("SQLService", "deleteItem failed: {}", lastError());
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
        LogService::warn("SQLService", "prepare get failed: {}", lastError());
        return results;
    }

    sqlite3_bind_int(stmt, 1, MAX_ITEM);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(makeContentItem(stmt));
    }

    sqlite3_finalize(stmt);
    return results;
}
