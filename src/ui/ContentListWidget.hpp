#pragma once

#include <QVector>
#include <QWidget>

#include "ContentListItemWidget.hpp"

class QVBoxLayout;
class QScrollArea;

class ContentListWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ContentListWidget(QWidget* parent = nullptr);

    void setItems(const QVector<ContentListItemData>& items);
    void scrollToTop();

signals:
    void itemClicked(const QString& content);

private:
    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;

    void clearItems();
};
