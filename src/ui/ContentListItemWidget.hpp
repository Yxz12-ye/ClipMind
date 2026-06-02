#pragma once

#include <QColor>
#include <QString>
#include <QWidget>
#include <QCryptographicHash>
#include "./TagWidget.hpp"

struct ContentListItemData      // 每个复制文本的数据
{
    Tag tag;
    QString content;
    QDateTime copyTime;
    QDateTime updateTime;
    QByteArray hash;
    bool pinned = false;

    ContentListItemData(const Tag& tg, QString _cnt, QDateTime _cpt = {}, QDateTime _upt = {})
    : tag(tg), content(_cnt), copyTime(_cpt), updateTime(_upt)
    {
        hash = QCryptographicHash::hash(content.toLocal8Bit(), QCryptographicHash::Sha256);
    }
};


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
