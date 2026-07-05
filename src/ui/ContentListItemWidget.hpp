#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <QCryptographicHash>
#include "./TagWidget.hpp"
#include "../struct.hpp"

class QLabel;
class QVBoxLayout;
class QHBoxLayout;

class ContentListItemWidget final : public QWidget {    // 复制文本的Widget
public:
    explicit ContentListItemWidget(QWidget* parent = nullptr);

    void setItemData(const ContentListItemData& data);

private:
    QWidget* m_badgeContainer;
    QLabel* m_badgeLabel;
    QLabel* m_timeLabel;
    QLabel* m_bodyLabel;
    QDateTime m_copyTime;
    QDateTime m_updateTime;
    QColor m_badgeBackground;
    QColor m_badgeForeground;

    void updateBadgeStyle() const;
    void updateTime();
};
