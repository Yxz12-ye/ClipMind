#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include "./TagWidget.hpp"

enum class SysContentItemKind {
    Text,
    Link,
    Code,
};

struct SysContentListItemData {
    SysContentItemKind kind;
    QString timeText;
    QString bodyText;
    bool pinned = false;
};

struct UsrContentListItemData
{
    SysContentItemKind kind;
    QString timeText;
    QString bodyText;
    bool pinned = false;
};


class QLabel;
class QVBoxLayout;
class QHBoxLayout;

class ContentListItemWidget final : public QWidget {
public:
    explicit ContentListItemWidget(QWidget* parent = nullptr);

    void setItemData(const SysContentListItemData& data);

private:
    QWidget* m_badgeContainer;
    QLabel* m_badgeLabel;
    QLabel* m_timeLabel;
    QLabel* m_bodyLabel;
    QColor m_badgeBackground;
    QColor m_badgeForeground;

    void updateBadgeStyle() const;
};
