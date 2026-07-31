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
class QEnterEvent;
class QEvent;
class QGraphicsDropShadowEffect;
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
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QGraphicsDropShadowEffect* m_shadowEffect;
    QWidget* m_badgeContainer;
    QLabel* m_badgeLabel;
    QLabel* m_timeLabel;
    QLabel* m_bodyLabel;
    QString m_bodyText;
    QDateTime m_copyTime;
    QDateTime m_updateTime;
    QColor m_badgeBackground;
    QColor m_badgeForeground;
    bool m_pinned = false;
    bool m_hovered = false;
    bool m_updatingTheme = false;

    void applyTheme();
    void refreshBodyText();
    void updateBadgeStyle() const;
    void updateShadowEffect();
    void updateTime();
};
