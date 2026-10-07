#include "ContentListItemWidget.hpp"

#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QPalette>
#include <QResizeEvent>
#include <QStringList>
#include <QTextLayout>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString elideToTwoLines(const QString& text, const QFont& font, int width) {
    if (text.isEmpty() || width <= 0) {
        return text;
    }

    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);

    QStringList visibleLines;
    QFontMetrics metrics(font);

    layout.beginLayout();
    for (int lineIndex = 0; lineIndex < 2; ++lineIndex) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLineWidth(width);

        const int start = line.textStart();
        const int length = line.textLength();
        const bool hasMoreText = start + length < text.size();

        QString lineText = text.mid(start, length);
        if (lineIndex == 1 && hasMoreText) {
            lineText = metrics.elidedText(text.mid(start), Qt::ElideRight, width);
        }

        visibleLines.append(lineText);

        if (!hasMoreText) {
            break;
        }
    }
    layout.endLayout();

    return visibleLines.join('\n');
}

}  // namespace

ContentListItemWidget::ContentListItemWidget(QWidget* parent)
    : QWidget(parent),
      m_shadowEffect(new QGraphicsDropShadowEffect),
      m_badgeContainer(new QWidget(this)),
      m_badgeLabel(new QLabel(m_badgeContainer)),
      m_timeLabel(new QLabel(this)),
      m_bodyLabel(new QLabel(this)),
      m_actionPanel(new QWidget(this)),
      m_pinButton(new QToolButton(m_actionPanel)),
      m_tagButton(new QToolButton(m_actionPanel)),
      m_deleteButton(new QToolButton(m_actionPanel)) {
    m_badgeContainer->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_badgeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_timeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_bodyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

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
    m_bodyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_bodyLabel->setFixedHeight(QFontMetrics(bodyFont).lineSpacing() * 2);

    auto* actionLayout = new QHBoxLayout(m_actionPanel);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(2);

    const QList<QToolButton*> actionButtons = {m_pinButton, m_tagButton, m_deleteButton};
    for (QToolButton* button : actionButtons) {
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        button->setMinimumHeight(48);
        button->setIconSize(QSize(26, 26));
        actionLayout->addWidget(button);
    }

    m_pinButton->setIcon(QIcon(QStringLiteral(":/img/pin.svg")));
    m_tagButton->setIcon(QIcon(QStringLiteral(":/img/tag.svg")));
    m_deleteButton->setIcon(QIcon(QStringLiteral(":/img/trash.svg")));
    m_actionPanel->setObjectName(QStringLiteral("itemActions"));
    for (QToolButton* button : actionButtons) {
        button->setObjectName(QStringLiteral("itemActionButton"));
    }
    m_actionPanel->hide();

    connect(m_pinButton, &QToolButton::clicked, this, [this] {
        setActionsVisible(false);
        emit pinRequested(m_hash, !m_pinned);
    });
    connect(m_tagButton, &QToolButton::clicked, this, [this] {
        const QPoint globalPos = m_tagButton->mapToGlobal(QPoint(0, m_tagButton->height()));
        emit tagChangeRequested(m_hash, m_tagName, globalPos);
    });
    connect(m_deleteButton, &QToolButton::clicked, this, [this] {
        setActionsVisible(false);
        emit deleteRequested(m_hash);
    });

    headerLayout->addWidget(m_badgeContainer, 0, Qt::AlignLeft | Qt::AlignVCenter);
    headerLayout->addStretch();
    headerLayout->addWidget(m_timeLabel, 0, Qt::AlignRight | Qt::AlignVCenter);

    rootLayout->addLayout(headerLayout);
    rootLayout->addWidget(m_bodyLabel);

    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("contentListItem");
    setCursor(Qt::PointingHandCursor);
    applyTheme();
    setGraphicsEffect(m_shadowEffect);
    updateShadowEffect();
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rootLayout->activate();
    const int itemHeight = rootLayout->sizeHint().height();
    setFixedHeight(itemHeight);
}

void ContentListItemWidget::setItemData(const ContentListItemData& data) {
    m_badgeBackground = data.tag.tagBackColor;
    m_badgeForeground = data.tag.tagNameColor;

    m_copyTime = data.copyTime;
    m_updateTime = data.updateTime;
    m_tagName = data.tag.tagName;
    m_hash = data.hash;

    m_badgeLabel->setText(data.tag.tagName);
    updateTime();
    m_bodyText = data.content;
    refreshBodyText();
    updateBadgeStyle();
    m_pinned = data.pinned;
    updateActionButtons();
    updateShadowEffect();
}

void ContentListItemWidget::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    m_hovered = true;
    updateShadowEffect();
}

void ContentListItemWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    const QPoint localCursorPosition = mapFromGlobal(QCursor::pos());
    m_hovered = rect().contains(localCursorPosition);
    if (!m_hovered) {
        setActionsVisible(false);
    }
    updateShadowEffect();
}

void ContentListItemWidget::changeEvent(QEvent* event) {
    if (m_updatingTheme && (event->type() == QEvent::PaletteChange ||
                            event->type() == QEvent::ApplicationPaletteChange)) {
        return;
    }
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        m_updatingTheme = true;
        applyTheme();
        m_updatingTheme = false;
    }

    QWidget::changeEvent(event);
}

void ContentListItemWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        setActionsVisible(true);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void ContentListItemWidget::mouseReleaseEvent(QMouseEvent* event) {
    QWidget::mouseReleaseEvent(event);

    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        emit clicked(m_bodyText);
    }
}

void ContentListItemWidget::setActionsVisible(bool visible) {
    m_badgeContainer->setVisible(!visible);
    m_timeLabel->setVisible(!visible);
    m_bodyLabel->setVisible(!visible);

    if (visible) {
        const int inset = 12;
        m_actionPanel->setGeometry(rect().adjusted(inset, inset, -inset, -inset));
        m_actionPanel->raise();
    }
    m_actionPanel->setVisible(visible);
    if (visible) {
        updateActionButtons();
    }
    updateGeometry();
}

void ContentListItemWidget::updateActionButtons() {
    m_pinButton->setToolTip(m_pinned ? QStringLiteral("取消固定") : QStringLiteral("固定"));
    m_tagButton->setToolTip(QStringLiteral("修改标签"));
    m_deleteButton->setToolTip(QStringLiteral("删除"));
}

void ContentListItemWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_actionPanel->isVisible()) {
        const int inset = 12;
        m_actionPanel->setGeometry(rect().adjusted(inset, inset, -inset, -inset));
        m_actionPanel->raise();
    }
    refreshBodyText();
}

void ContentListItemWidget::refreshBodyText() {
    const int availableWidth = m_bodyLabel->contentsRect().width() > 0
                                   ? m_bodyLabel->contentsRect().width()
                                   : m_bodyLabel->width();
    m_bodyLabel->setText(elideToTwoLines(m_bodyText, m_bodyLabel->font(), availableWidth));
}

void ContentListItemWidget::updateBadgeStyle() const {
    m_badgeContainer->setStyleSheet(QString("QWidget {"
                                            "background-color: %1;"
                                            "border-radius: 4px;"
                                            "}")
                                        .arg(m_badgeBackground.name()));

    m_badgeLabel->setStyleSheet(QString("color: %1;").arg(m_badgeForeground.name()));
}

void ContentListItemWidget::applyTheme() {
    const bool darkMode = palette().color(QPalette::Window).lightness() < 128;
    const QString background = darkMode ? "#252525" : "#FFFFFF";
    const QString border = darkMode ? "#383838" : "#E2E8F0";
    const QString bodyColor = darkMode ? "#F1F5F9" : "#1E293B";

    setStyleSheet(QString("#contentListItem {"
                          "background-color: %1;"
                          "border: 1px solid %2;"
                          "border-radius: 10px;"
                          "}"
                          "QWidget#itemActions { background: transparent; }"
                          "QToolButton#itemActionButton { border: none; border-radius: 6px; "
                          "background-color: %3; padding: 8px; }"
                          "QToolButton#itemActionButton:hover { background-color: %4; }")
                      .arg(background, border,
                           darkMode ? QStringLiteral("#3A3A3A") : QStringLiteral("#E2E8F0"),
                           darkMode ? QStringLiteral("#4A4A4A") : QStringLiteral("#CBD5E1")));
    m_timeLabel->setStyleSheet("color: #94A3B8;");
    m_bodyLabel->setStyleSheet(QString("color: %1;").arg(bodyColor));
}

void ContentListItemWidget::updateShadowEffect() {
    const bool darkMode = palette().color(QPalette::Window).lightness() < 128;

    if (m_hovered) {
        m_shadowEffect->setEnabled(true);
        m_shadowEffect->setBlurRadius(12.0);
        m_shadowEffect->setOffset(0.0, 3.0);
        m_shadowEffect->setColor(darkMode ? QColor(0, 0, 0, 96) : QColor(15, 23, 42, 48));
        return;
    }

    if (m_pinned) {
        m_shadowEffect->setEnabled(true);
        m_shadowEffect->setBlurRadius(8.0);
        m_shadowEffect->setOffset(0.0, 2.0);
        m_shadowEffect->setColor(darkMode ? QColor(0, 0, 0, 48) : QColor(0, 0, 0, 16));
        return;
    }

    m_shadowEffect->setEnabled(false);
}

void ContentListItemWidget::updateTime() {
    if (!m_updateTime.isValid()) {
        m_timeLabel->clear();
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const qint64 secs = m_updateTime.secsTo(now);  // 正数表示过去

    // 未来时间：直接显示完整日期时间（简洁起见）
    if (secs < 0) {
        m_timeLabel->setText(m_updateTime.toString("yyyy/MM/dd HH:mm:ss"));
        return;
    }

    const qint64 MINUTE = 60;
    const qint64 HOUR = 3600;
    const qint64 DAY = 86400;
    const qint64 MONTH = 30 * DAY;

    if (secs < MINUTE) {
        m_timeLabel->setText(QStringLiteral("刚刚"));
    } else if (secs < HOUR) {
        int minutes = static_cast<int>(secs / MINUTE);
        m_timeLabel->setText(QString::number(minutes) + QStringLiteral("分钟前"));
    } else if (secs < DAY) {
        int hours = static_cast<int>(secs / HOUR);
        m_timeLabel->setText(QString::number(hours) + QStringLiteral("小时前"));
    } else if (secs < MONTH) {
        int days = static_cast<int>(secs / DAY);
        m_timeLabel->setText(QString::number(days) + QStringLiteral("天前"));
    } else {
        int months = static_cast<int>(secs / MONTH);  // 按30天折算月数
        if (months < 12) {
            m_timeLabel->setText(QString::number(months) + QStringLiteral("个月前"));
        } else {
            int years = months / 12;
            m_timeLabel->setText(QString::number(years) + QStringLiteral("年前"));
        }
    }
}
