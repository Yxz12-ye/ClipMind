#include "ContentListWidget.hpp"

#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>

ContentListWidget::ContentListWidget(QWidget* parent)
    : QWidget(parent),
      m_contentWidget(new QWidget(this)),
      m_contentLayout(new QVBoxLayout(m_contentWidget)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->viewport()->setAutoFillBackground(false);
    scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea QWidget#qt_scrollarea_viewport { background: transparent; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }");

    m_contentLayout->setContentsMargins(12, 0, 12, 12);
    m_contentLayout->setSpacing(8);
    m_contentWidget->setAttribute(Qt::WA_StyledBackground, true);

    scrollArea->setWidget(m_contentWidget);
    rootLayout->addWidget(scrollArea);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ContentListWidget::setItems(const QVector<ContentListItemData>& items) {
    clearItems();

    for (const auto& item : items) {
        auto* itemWidget = new ContentListItemWidget(m_contentWidget);
        itemWidget->setItemData(item);
        connect(itemWidget, &ContentListItemWidget::clicked, this, &ContentListWidget::itemClicked);
        m_contentLayout->addWidget(itemWidget);
    }

    m_contentLayout->addStretch();
}

void ContentListWidget::clearItems() {
    QLayoutItem* child = nullptr;
    while ((child = m_contentLayout->takeAt(0)) != nullptr) {
        if (QWidget* widget = child->widget()) {
            widget->deleteLater();
        }
        delete child;
    }
}
