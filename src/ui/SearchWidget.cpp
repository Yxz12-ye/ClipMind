#include "SearchWidget.hpp"

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
    QWidget *roundedBg = new QWidget(this);
    roundedBg->setFixedSize(328, 36);
    // 设置背景色、圆角边框（可视需要加上边框）
    roundedBg->setStyleSheet(
        "QWidget {"
        "   background-color: #F8FAFC;"
        "   border-radius: 8px;"
        "   border: 1px solid #E2E8F0;"
        "}"
    );

    lineEdit = new QLineEdit(roundedBg);

    lineEdit->setStyleSheet(
        "QLineEdit {"
        "   border: none;"
        "   background: transparent;"
        "   padding: 0 8px;"
        "}"
    );

    lineEdit->setFont(QFont("Microsoft YaHei", 13));
    lineEdit->setPlaceholderText("搜索剪贴板...");

    QHBoxLayout *bgLayout = new QHBoxLayout(roundedBg);
    bgLayout->setContentsMargins(0, 0, 0, 0);  // 边距为0，使子控件完全填充
    bgLayout->addWidget(lineEdit);

    layout.setContentsMargins(0,0,0,0);
    layout.addWidget(roundedBg, 0, Qt::AlignCenter);
    setLayout(&layout);

    timer.setSingleShot(true);
    connect(lineEdit, &QLineEdit::textChanged, this, &SearchWidget::onTextChanged);
    connect(&timer, &QTimer::timeout, this, [this]() {
        emit inputTextChanged(lineEdit->text());
    });
}

void SearchWidget::focusInput()
{
    lineEdit->setFocus(Qt::ShortcutFocusReason);
    lineEdit->selectAll();
}
