#pragma once

#include <QString>

struct sqlite3;

class SQLiteDatabase {
public:
    SQLiteDatabase();
    ~SQLiteDatabase();

    SQLiteDatabase(const SQLiteDatabase&) = delete;
    SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;

    sqlite3* handle() const;
    QString databasePath() const;

private:
    sqlite3* m_db = nullptr;
    QString m_databasePath;

    void open();
    void initializeSchema();
};
