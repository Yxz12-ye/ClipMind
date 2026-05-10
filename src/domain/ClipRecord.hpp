#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

#include "domain/ContentKind.hpp"
#include "domain/Tag.hpp"

using ClipId = qint64;

struct ClipRecord {
    ClipId id = 0;
    ContentKind kind = ContentKind::Text;
    QString content;
    QDateTime createdAt;
    QDateTime updatedAt;
    QVector<TagId> tagIds;
};

struct ClipDraft {
    ContentKind kind = ContentKind::Text;
    QString content;
    QDateTime createdAt;
    QDateTime updatedAt;
    QVector<TagId> tagIds;
};
