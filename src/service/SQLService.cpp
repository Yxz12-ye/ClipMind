#include "SQLService.hpp"

sqlite3_int64 SQLService::searchTag(const QString& tagName) {
    int rc = SQLITE_OK;
    sqlite3_int64 tagId = -1;
    rc = sqlite3_bind_text(searchTagStmt, 1, tagName.toUtf8().constData(), tagName.size(), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        qWarning() << "bind failed:" << sqlite3_errmsg(db);
        sqlite3_reset(searchTagStmt);
        return -2;
    }

    // 3. 执行查询
    rc = sqlite3_step(searchTagStmt);
    if (rc == SQLITE_ROW) {
        // 找到记录，取出第一列（id）
        tagId = sqlite3_column_int64(searchTagStmt, 0);
    } else if (rc == SQLITE_DONE) {
        // 没有匹配的记录，tagId 保持 -1
        // 可以打印日志：未找到该标签
    } else {
        // 其他错误
        qWarning() << "step error:" << sqlite3_errmsg(db);
        sqlite3_finalize(searchTagStmt);
        return -2;
    }

    // 4. 释放资源
    sqlite3_reset(searchTagStmt);
    return tagId;
}

SQLService::SQLService(QObject* parent) : QObject(parent) {
    databaseDir = QDir(QDir::homePath() + DATABASE_DIR);
    if (!databaseDir.exists()) {
        if (QDir().mkpath(databaseDir.absolutePath())) {
            qDebug() << "成功创建目录";
        } else {
            qWarning() << "创建目录失败";
            return;
        }
    }
    int rc = SQLITE_OK;
    rc = sqlite3_open(databaseDir.absolutePath().toUtf8() + DATABASE_NAME, &db);
    if (rc) {
        // 打开失败 使用sqlite3_errmsg(db)获取错误信息
        return ;
    }
    sqlite3_stmt* tagStmt;
    char* errMsg = nullptr;
    rc = sqlite3_exec(db, TABLE_TAG, nullptr, nullptr, &errMsg);
    if (rc) {
        return ;
    }
    rc = sqlite3_exec(db, TABLE_CONTENT, nullptr, nullptr, &errMsg);
    if (rc) {
        return ;
    }
    sqlite3_prepare_v2(db, tagSQL, -1, &tagStmt, nullptr);
    sqlite3_prepare_v2(db, contentSQL, -1, &contentStmt, nullptr);
    sqlite3_prepare_v2(db, sqlSearchTag, -1, &searchTagStmt, nullptr);
}

SQLService::~SQLService() {
    sqlite3_close(db);
    sqlite3_finalize(tagStmt);
    sqlite3_finalize(contentStmt);
    sqlite3_finalize(searchTagStmt);
}

QString SQLService::save(const ContentListItemData& data) {
    sqlite3_int64 tagId = searchTag(data.tag.tagName);

    sqlite3_bind_int64(contentStmt, 1, tagId);
    sqlite3_bind_text(contentStmt, 2, data.content.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(contentStmt, 3, data.copyTime.toSecsSinceEpoch());   // 秒级时间戳
    sqlite3_bind_int64(contentStmt, 4, data.updateTime.toSecsSinceEpoch());
    sqlite3_bind_blob(contentStmt, 5, data.hash.constData(), data.hash.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(contentStmt, 6, data.pinned ? 1 : 0);

    if (sqlite3_step(tagStmt) != SQLITE_DONE) {
        // 错误处理...
    }
    sqlite3_reset(tagStmt);
}
