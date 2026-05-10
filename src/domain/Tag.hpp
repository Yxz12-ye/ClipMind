#pragma once

#include <QDateTime>
#include <QString>

using TagId = qint64;

struct Tag {
    TagId id = 0;
    QString name;
    QString color;
    int sortOrder = 0;
    QString description;
    QDateTime createdAt;
    QDateTime updatedAt;
    bool isBuiltin = false;
};

struct TagDraft {
    QString name;
    QString color;
    int sortOrder = 0;
    QString description;
};
