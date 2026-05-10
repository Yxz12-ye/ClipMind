#include "data/SQLiteClipRepository.hpp"

#include <stdexcept>

#include <QDateTime>

#include <sqlite3.h>

#include "domain/ContentKind.hpp"

namespace {
class Statement final {
public:
    Statement(sqlite3* db, const QString& sql) {
        const QByteArray encoded = sql.toUtf8();
        const int rc = sqlite3_prepare_v2(db, encoded.constData(), -1, &m_statement, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(db));
        }
    }

    ~Statement() {
        if (m_statement != nullptr) {
            sqlite3_finalize(m_statement);
        }
    }

    sqlite3_stmt* get() const {
        return m_statement;
    }

private:
    sqlite3_stmt* m_statement = nullptr;
};

void bindText(sqlite3_stmt* statement, int index, const QString& value) {
    sqlite3_bind_text(statement, index, value.toUtf8().constData(), -1, SQLITE_TRANSIENT);
}

ClipRecord readClip(sqlite3_stmt* statement) {
    ClipRecord clip;
    clip.id = sqlite3_column_int64(statement, 0);
    clip.kind = contentKindFromStorageString(
        QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement, 1))));
    clip.content = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)));
    clip.createdAt = QDateTime::fromString(
        QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement, 3))),
        Qt::ISODate);
    clip.updatedAt = QDateTime::fromString(
        QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement, 4))),
        Qt::ISODate);
    return clip;
}

QString buildBaseQuery(const ClipFilter& filter, const QString& selectClause) {
    QString query = selectClause;
    if (filter.tagId.has_value()) {
        query += " INNER JOIN clip_tags ct ON ct.clip_id = c.id";
    }
    query += " WHERE 1 = 1";
    if (filter.kind.has_value()) {
        query += " AND c.kind = ?1";
        if (filter.tagId.has_value()) {
            query += " AND ct.tag_id = ?2";
        }
    } else if (filter.tagId.has_value()) {
        query += " AND ct.tag_id = ?1";
    }
    return query;
}

void bindFilter(sqlite3_stmt* statement, const ClipFilter& filter) {
    int index = 1;
    if (filter.kind.has_value()) {
        bindText(statement, index++, contentKindToStorageString(*filter.kind));
    }
    if (filter.tagId.has_value()) {
        sqlite3_bind_int64(statement, index, *filter.tagId);
    }
}
}

SQLiteClipRepository::SQLiteClipRepository(SQLiteDatabase& database)
    : m_database(database) {
}

QVector<ClipRecord> SQLiteClipRepository::listClips(const ClipFilter& filter) const {
    const QString query = buildBaseQuery(
        filter,
        "SELECT DISTINCT c.id, c.kind, c.content, c.created_at, c.updated_at FROM clips c")
        + " ORDER BY c.created_at DESC;";
    Statement statement(m_database.handle(), query);
    bindFilter(statement.get(), filter);

    QVector<ClipRecord> clips;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        ClipRecord clip = readClip(statement.get());
        clip.tagIds = listTagIdsForClip(clip.id);
        clips.push_back(clip);
    }

    return clips;
}

int SQLiteClipRepository::countClips(const ClipFilter& filter) const {
    const QString query = buildBaseQuery(
        filter,
        "SELECT COUNT(DISTINCT c.id) FROM clips c");
    Statement statement(m_database.handle(), query);
    bindFilter(statement.get(), filter);

    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
        return sqlite3_column_int(statement.get(), 0);
    }

    return 0;
}

ClipRecord SQLiteClipRepository::createClip(const ClipDraft& draft) {
    const QString createdAt = draft.createdAt.isValid()
        ? draft.createdAt.toUTC().toString(Qt::ISODate)
        : QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const QString updatedAt = draft.updatedAt.isValid()
        ? draft.updatedAt.toUTC().toString(Qt::ISODate)
        : createdAt;

    Statement statement(
        m_database.handle(),
        "INSERT INTO clips(kind, content, created_at, updated_at) VALUES(?1, ?2, ?3, ?4);");
    bindText(statement.get(), 1, contentKindToStorageString(draft.kind));
    bindText(statement.get(), 2, draft.content);
    bindText(statement.get(), 3, createdAt);
    bindText(statement.get(), 4, updatedAt);

    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(m_database.handle()));
    }

    const ClipId clipId = sqlite3_last_insert_rowid(m_database.handle());
    for (const TagId tagId : draft.tagIds) {
        attachTag(clipId, tagId);
    }

    return findClipById(clipId);
}

void SQLiteClipRepository::attachTag(ClipId clipId, TagId tagId) {
    Statement statement(
        m_database.handle(),
        "INSERT OR IGNORE INTO clip_tags(clip_id, tag_id, created_at) VALUES(?1, ?2, ?3);");
    sqlite3_bind_int64(statement.get(), 1, clipId);
    sqlite3_bind_int64(statement.get(), 2, tagId);
    bindText(statement.get(), 3, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(m_database.handle()));
    }
}

void SQLiteClipRepository::detachTag(ClipId clipId, TagId tagId) {
    Statement statement(
        m_database.handle(),
        "DELETE FROM clip_tags WHERE clip_id = ?1 AND tag_id = ?2;");
    sqlite3_bind_int64(statement.get(), 1, clipId);
    sqlite3_bind_int64(statement.get(), 2, tagId);

    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(m_database.handle()));
    }
}

QVector<TagId> SQLiteClipRepository::listTagIdsForClip(ClipId clipId) const {
    Statement statement(
        m_database.handle(),
        "SELECT tag_id FROM clip_tags WHERE clip_id = ?1 ORDER BY tag_id ASC;");
    sqlite3_bind_int64(statement.get(), 1, clipId);

    QVector<TagId> tagIds;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        tagIds.push_back(sqlite3_column_int64(statement.get(), 0));
    }

    return tagIds;
}

ClipRecord SQLiteClipRepository::findClipById(ClipId clipId) const {
    Statement statement(
        m_database.handle(),
        "SELECT id, kind, content, created_at, updated_at FROM clips WHERE id = ?1;");
    sqlite3_bind_int64(statement.get(), 1, clipId);

    if (sqlite3_step(statement.get()) != SQLITE_ROW) {
        throw std::runtime_error("Clip not found after insert");
    }

    ClipRecord clip = readClip(statement.get());
    clip.tagIds = listTagIdsForClip(clip.id);
    return clip;
}
