#pragma once

#include <QVector>
#include <QWidget>

#include "ContentListItemWidget.hpp"

class QVBoxLayout;
class QScrollArea;

class ContentListWidget final : public QWidget {
public:
    explicit ContentListWidget(QWidget* parent = nullptr);

    void setItems(const QVector<ContentListItemData>& items);

private:
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;

    void clearItems();
};
