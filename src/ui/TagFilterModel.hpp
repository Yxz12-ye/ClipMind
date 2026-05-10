#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "service/TagService.hpp"

class TagFilterModel final : public QAbstractListModel {
public:
    enum Roles {
        FilterKindRole = Qt::UserRole + 1,
        ContentKindRole,
        TagIdRole,
        CountRole,
        ColorRole,
        DescriptionRole,
    };

    explicit TagFilterModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setItems(QVector<TagFilterItem> items);
    const TagFilterItem* itemAt(const QModelIndex& index) const;

private:
    QVector<TagFilterItem> m_items;
};
