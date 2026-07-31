#include "SearchWidget.hpp"

#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QPalette>
#include <QWidget>

void SearchWidget::onTextChanged(const QString& text) {
    if (text.isEmpty()) {
        timer.stop();
        emit inputTextChanged(QString());
        return;
    }
    // 消除输入抖动
    timer.start(1000);
}

SearchWidget::SearchWidget(QWidget* parent) : QWidget(parent), layout(this) {
    roundedBg = new QWidget(this);
    roundedBg->setFixedSize(328, 36);

    lineEdit = new QLineEdit(roundedBg);
    lineEdit->setFont(QFont("Microsoft YaHei", 13));
    lineEdit->setPlaceholderText("搜索剪贴板...");

    QHBoxLayout* bgLayout = new QHBoxLayout(roundedBg);
    bgLayout->setContentsMargins(0, 0, 0, 0);  // 边距为0，使子控件完全填充
    bgLayout->addWidget(lineEdit);

    layout.setContentsMargins(0, 0, 0, 0);
    layout.addWidget(roundedBg, 0, Qt::AlignCenter);
    setLayout(&layout);

    timer.setSingleShot(true);
    connect(lineEdit, &QLineEdit::textChanged, this, &SearchWidget::onTextChanged);
    connect(&timer, &QTimer::timeout, this, [this]() { emit inputTextChanged(lineEdit->text()); });

    applyTheme();
}

void SearchWidget::applyTheme() {
    const bool darkMode = palette().color(QPalette::Window).lightness() < 128;
    const QString background = darkMode ? "#252525" : "#F8FAFC";
    const QString border = darkMode ? "#383838" : "#E2E8F0";
    const QString text = darkMode ? "#F1F5F9" : "#0F172A";
    const QString placeholder = darkMode ? "#94A3B8" : "#64748B";

    roundedBg->setStyleSheet(QString("QWidget {"
                                     "background-color: %1;"
                                     "border-radius: 8px;"
                                     "border: 1px solid %2;"
                                     "}")
                                 .arg(background, border));
    lineEdit->setStyleSheet(QString("QLineEdit {"
                                    "border: none;"
                                    "background: transparent;"
                                    "color: %1;"
                                    "selection-background-color: #3B82F6;"
                                    "selection-color: #FFFFFF;"
                                    "padding: 0 8px;"
                                    "placeholder-text-color: %2;"
                                    "}")
                                .arg(text, placeholder));
}

void SearchWidget::changeEvent(QEvent* event) {
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

void SearchWidget::focusInput() {
    lineEdit->setFocus(Qt::ShortcutFocusReason);
    lineEdit->selectAll();
}
