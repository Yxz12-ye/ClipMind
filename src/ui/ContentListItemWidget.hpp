#pragma once

#include <QColor>
#include <QCryptographicHash>
#include <QString>
#include <QWidget>

#include "../struct.hpp"
#include "./TagWidget.hpp"

class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QMouseEvent;
class QResizeEvent;

class ContentListItemWidget final : public QWidget {  // 复制文本的Widget
    Q_OBJECT

public:
    explicit ContentListItemWidget(QWidget* parent = nullptr);

    void setItemData(const ContentListItemData& data);

signals:
    void clicked(const QString& content);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QWidget* m_badgeContainer;
    QLabel* m_badgeLabel;
    QLabel* m_timeLabel;
    QLabel* m_bodyLabel;
    QString m_bodyText;
    QDateTime m_copyTime;
    QDateTime m_updateTime;
    QColor m_badgeBackground;
    QColor m_badgeForeground;

    void refreshBodyText();
    void updateBadgeStyle() const;
    void updateTime();
};
