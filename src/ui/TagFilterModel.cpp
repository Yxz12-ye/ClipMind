#include "ui/TagFilterModel.hpp"

#include <utility>

TagFilterModel::TagFilterModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int TagFilterModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return m_items.size();
}

QVariant TagFilterModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const TagFilterItem& item = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return item.label;
    case FilterKindRole:
        return static_cast<int>(item.kind);
    case ContentKindRole:
        return item.contentKind.has_value() ? QVariant::fromValue(static_cast<int>(*item.contentKind)) : QVariant();
    case TagIdRole:
        return item.tagId.has_value() ? QVariant::fromValue(*item.tagId) : QVariant();
    case CountRole:
        return item.count;
    case ColorRole:
        return item.color;
    case DescriptionRole:
        return item.description;
    default:
        return {};
    }
}

void TagFilterModel::setItems(QVector<TagFilterItem> items) {
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
}

const TagFilterItem* TagFilterModel::itemAt(const QModelIndex& index) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return nullptr;
    }

    return &m_items.at(index.row());
}
