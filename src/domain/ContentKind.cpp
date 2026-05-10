#include "domain/ContentKind.hpp"

#include <stdexcept>

QString contentKindToStorageString(ContentKind kind) {
    switch (kind) {
    case ContentKind::Text:
        return "text";
    case ContentKind::Link:
        return "link";
    case ContentKind::Code:
        return "code";
    }

    return "text";
}

QString contentKindToDisplayName(ContentKind kind) {
    switch (kind) {
    case ContentKind::Text:
        return QStringLiteral("文本");
    case ContentKind::Link:
        return QStringLiteral("链接");
    case ContentKind::Code:
        return QStringLiteral("代码");
    }

    return QStringLiteral("文本");
}

QString contentKindToBadgeText(ContentKind kind) {
    return contentKindToStorageString(kind).toUpper();
}

ContentKind contentKindFromStorageString(const QString& value) {
    if (value == "text") {
        return ContentKind::Text;
    }
    if (value == "link") {
        return ContentKind::Link;
    }
    if (value == "code") {
        return ContentKind::Code;
    }

    throw std::invalid_argument("Unsupported content kind");
}
