#include "ContentListItemWidget.hpp"

#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>

namespace {
QString badgeTextForKind(SysContentItemKind kind) {
    switch (kind) {
    case SysContentItemKind::Text:
        return "TEXT";
    case SysContentItemKind::Link:
        return "LINK";
    case SysContentItemKind::Code:
        return "CODE";
    }

    return "TEXT";
}
}

ContentListItemWidget::ContentListItemWidget(QWidget* parent)
    : QWidget(parent),
      m_badgeContainer(new QWidget(this)),
      m_badgeLabel(new QLabel(m_badgeContainer)),
      m_timeLabel(new QLabel(this)),
      m_bodyLabel(new QLabel(this)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(8);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    auto* badgeLayout = new QHBoxLayout(m_badgeContainer);
    badgeLayout->setContentsMargins(6, 2, 6, 2);
    badgeLayout->addWidget(m_badgeLabel);

    QFont badgeFont("Microsoft YaHei UI", 9);
    badgeFont.setBold(true);
    m_badgeLabel->setFont(badgeFont);

    QFont timeFont("Microsoft YaHei UI", 11);
    m_timeLabel->setFont(timeFont);
    m_timeLabel->setStyleSheet("color: #94A3B8;");

    QFont bodyFont("Microsoft YaHei UI", 13);
    m_bodyLabel->setFont(bodyFont);
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    headerLayout->addWidget(m_badgeContainer, 0, Qt::AlignLeft | Qt::AlignVCenter);
    headerLayout->addStretch();
    headerLayout->addWidget(m_timeLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    rootLayout->addLayout(headerLayout);
    rootLayout->addWidget(m_bodyLabel);

    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("contentListItem");
    setStyleSheet(
        "#contentListItem {"
        "background-color: #FFFFFF;"
        "border: 1px solid #E2E8F0;"
        "border-radius: 10px;"
        "}"
    );
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
}

void ContentListItemWidget::setItemData(const SysContentListItemData& data) {
    QColor bodyColor("#1E293B");

    switch (data.kind) {
    case SysContentItemKind::Text:
        m_badgeBackground = QColor("#EFF6FF");
        m_badgeForeground = QColor("#3B82F6");
        break;
    case SysContentItemKind::Link:
        m_badgeBackground = QColor("#F0FDF4");
        m_badgeForeground = QColor("#16A34A");
        bodyColor = QColor("#3B82F6");
        break;
    case SysContentItemKind::Code:
        m_badgeBackground = QColor("#FEF9C3");
        m_badgeForeground = QColor("#CA8A04");
        break;
    }

    m_badgeLabel->setText(badgeTextForKind(data.kind));
    m_timeLabel->setText(data.timeText);
    m_bodyLabel->setText(data.bodyText);
    m_bodyLabel->setStyleSheet(QString("color: %1;").arg(bodyColor.name()));
    updateBadgeStyle();

    if (auto* existingEffect = graphicsEffect()) {
        setGraphicsEffect(nullptr);
        existingEffect->deleteLater();
    }

    if (data.pinned) {
        auto* shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(8.0);
        shadow->setOffset(0.0, 2.0);
        shadow->setColor(QColor(0, 0, 0, 16));
        setGraphicsEffect(shadow);
    }
}

void ContentListItemWidget::updateBadgeStyle() const {
    m_badgeContainer->setStyleSheet(QString(
        "QWidget {"
        "background-color: %1;"
        "border-radius: 4px;"
        "}"
    ).arg(m_badgeBackground.name()));

    m_badgeLabel->setStyleSheet(QString("color: %1;").arg(m_badgeForeground.name()));
}
