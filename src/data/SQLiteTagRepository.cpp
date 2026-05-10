#include "data/SQLiteTagRepository.hpp"

#include <stdexcept>

#include <QDateTime>

#include <sqlite3.h>

namespace {
class Statement final {
public:
    Statement(sqlite3* db, const char* sql) {
        const int rc = sqlite3_prepare_v2(db, sql, -1, &m_statement, nullptr);
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

Tag readTag(sqlite3_stmt* statement) {
    Tag tag;
    tag.id = sqlite3_column_int64(statement, 0);
    tag.name = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)));
    tag.color = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)));
    tag.sortOrder = sqlite3_column_int(statement, 3);
    const unsigned char* description = sqlite3_column_text(statement, 4);
    if (description != nullptr) {
        tag.description = QString::fromUtf8(reinterpret_cast<const char*>(description));
    }
    tag.createdAt = QDateTime::fromString(
        QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement, 5))),
        Qt::ISODate);
    tag.updatedAt = QDateTime::fromString(
        QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(statement, 6))),
        Qt::ISODate);
    return tag;
}

void bindText(sqlite3_stmt* statement, int index, const QString& value) {
    sqlite3_bind_text(statement, index, value.toUtf8().constData(), -1, SQLITE_TRANSIENT);
}
}

SQLiteTagRepository::SQLiteTagRepository(SQLiteDatabase& database)
    : m_database(database) {
}

QVector<Tag> SQLiteTagRepository::listTags() const {
    Statement statement(m_database.handle(),
        "SELECT id, name, color, sort_order, description, created_at, updated_at "
        "FROM tags ORDER BY sort_order ASC, name COLLATE NOCASE ASC;");

    QVector<Tag> tags;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        tags.push_back(readTag(statement.get()));
    }

    return tags;
}

std::optional<Tag> SQLiteTagRepository::findById(TagId id) const {
    Statement statement(m_database.handle(),
        "SELECT id, name, color, sort_order, description, created_at, updated_at "
        "FROM tags WHERE id = ?1;");
    sqlite3_bind_int64(statement.get(), 1, id);

    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
        return readTag(statement.get());
    }

    return std::nullopt;
}

std::optional<Tag> SQLiteTagRepository::findByNameInsensitive(const QString& name) const {
    Statement statement(m_database.handle(),
        "SELECT id, name, color, sort_order, description, created_at, updated_at "
        "FROM tags WHERE name = ?1 COLLATE NOCASE;");
    bindText(statement.get(), 1, name);

    if (sqlite3_step(statement.get()) == SQLITE_ROW) {
        return readTag(statement.get());
    }

    return std::nullopt;
}

Tag SQLiteTagRepository::createTag(const TagDraft& draft) {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString timestamp = now.toString(Qt::ISODate);

    Statement statement(m_database.handle(),
        "INSERT INTO tags(name, color, sort_order, description, created_at, updated_at) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6);");
    bindText(statement.get(), 1, draft.name);
    bindText(statement.get(), 2, draft.color);
    sqlite3_bind_int(statement.get(), 3, draft.sortOrder);
    if (draft.description.isEmpty()) {
        sqlite3_bind_null(statement.get(), 4);
    } else {
        bindText(statement.get(), 4, draft.description);
    }
    bindText(statement.get(), 5, timestamp);
    bindText(statement.get(), 6, timestamp);

    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(m_database.handle()));
    }

    const TagId id = sqlite3_last_insert_rowid(m_database.handle());
    return findById(id).value();
}

Tag SQLiteTagRepository::updateTag(const Tag& tag) {
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    Statement statement(m_database.handle(),
        "UPDATE tags SET name = ?1, color = ?2, sort_order = ?3, description = ?4, updated_at = ?5 "
        "WHERE id = ?6;");
    bindText(statement.get(), 1, tag.name);
    bindText(statement.get(), 2, tag.color);
    sqlite3_bind_int(statement.get(), 3, tag.sortOrder);
    if (tag.description.isEmpty()) {
        sqlite3_bind_null(statement.get(), 4);
    } else {
        bindText(statement.get(), 4, tag.description);
    }
    bindText(statement.get(), 5, timestamp);
    sqlite3_bind_int64(statement.get(), 6, tag.id);

    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(m_database.handle()));
    }

    return findById(tag.id).value();
}

void SQLiteTagRepository::deleteTag(TagId id) {
    Statement statement(m_database.handle(), "DELETE FROM tags WHERE id = ?1;");
    sqlite3_bind_int64(statement.get(), 1, id);

    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(m_database.handle()));
    }
}
