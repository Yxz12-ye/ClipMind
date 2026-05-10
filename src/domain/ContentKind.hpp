#pragma once

#include <QString>

enum class ContentKind {
    Text = 0,
    Link,
    Code,
};

QString contentKindToStorageString(ContentKind kind);
QString contentKindToDisplayName(ContentKind kind);
QString contentKindToBadgeText(ContentKind kind);
ContentKind contentKindFromStorageString(const QString& value);
