#include "data/SQLiteDatabase.hpp"

#include <stdexcept>

#include <QDir>
#include <QStandardPaths>

#include <sqlite3.h>

namespace {
void executeOrThrow(sqlite3* db, const char* sql) {
    char* errorMessage = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errorMessage);
    if (rc != SQLITE_OK) {
        const QString message = errorMessage != nullptr ? QString::fromUtf8(errorMessage) : QStringLiteral("unknown sqlite error");
        sqlite3_free(errorMessage);
        throw std::runtime_error(message.toStdString());
    }
}
}

SQLiteDatabase::SQLiteDatabase() {
    open();
    initializeSchema();
}

SQLiteDatabase::~SQLiteDatabase() {
    if (m_db != nullptr) {
        sqlite3_close(m_db);
    }
}

sqlite3* SQLiteDatabase::handle() const {
    return m_db;
}

QString SQLiteDatabase::databasePath() const {
    return m_databasePath;
}

void SQLiteDatabase::open() {
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    m_databasePath = appDataPath + QStringLiteral("/clipmind.db");

    const int rc = sqlite3_open_v2(
        m_databasePath.toUtf8().constData(),
        &m_db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr);
    if (rc != SQLITE_OK) {
        const QString message = m_db != nullptr ? QString::fromUtf8(sqlite3_errmsg(m_db)) : QStringLiteral("failed to open sqlite database");
        throw std::runtime_error(message.toStdString());
    }
}

void SQLiteDatabase::initializeSchema() {
    executeOrThrow(m_db, "PRAGMA foreign_keys = ON;");
    executeOrThrow(
        m_db,
        "CREATE TABLE IF NOT EXISTS tags ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL COLLATE NOCASE UNIQUE,"
        "color TEXT NOT NULL,"
        "sort_order INTEGER NOT NULL DEFAULT 0,"
        "description TEXT NULL,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ");");
    executeOrThrow(
        m_db,
        "CREATE TABLE IF NOT EXISTS clips ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "kind TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "created_at TEXT NOT NULL,"
        "updated_at TEXT NOT NULL"
        ");");
    executeOrThrow(
        m_db,
        "CREATE TABLE IF NOT EXISTS clip_tags ("
        "clip_id INTEGER NOT NULL,"
        "tag_id INTEGER NOT NULL,"
        "created_at TEXT NOT NULL,"
        "PRIMARY KEY (clip_id, tag_id),"
        "FOREIGN KEY (clip_id) REFERENCES clips(id) ON DELETE CASCADE,"
        "FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE"
        ");");
}
