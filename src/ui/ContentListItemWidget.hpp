#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

enum class ContentItemKind {
    Text,
    Link,
    Code,
};

struct ContentListItemData {
    ContentItemKind kind;
    QString timeText;
    QString bodyText;
    bool elevated = false;
};

class QLabel;
class QVBoxLayout;
class QHBoxLayout;

class ContentListItemWidget final : public QWidget {
public:
    explicit ContentListItemWidget(QWidget* parent = nullptr);

    void setItemData(const ContentListItemData& data);

private:
    QWidget* m_badgeContainer;
    QLabel* m_badgeLabel;
    QLabel* m_timeLabel;
    QLabel* m_bodyLabel;
    QColor m_badgeBackground;
    QColor m_badgeForeground;

    void updateBadgeStyle() const;
};
