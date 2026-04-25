#include "CustomHead.hpp"

#include <QIcon>
#include <QWindow>

void CustomHead::setupUI() {
    icon.load(QString(":/img/icon.svg"));
    icon.setFixedSize(16, 16);
    closeButton.setFixedSize(18, 18);
    closeButton.setIcon(QIcon(":/img/x.svg"));
    closeButton.setIconSize(QSize(14, 14));
    closeButton.setAutoRaise(true);
    closeButton.setCursor(Qt::PointingHandCursor);
    layout.setContentsMargins(QMargins(0, 0, 0, 0));
    layout.addSpacing(16);
    layout.addWidget(&icon);
    layout.addSpacing(8);
    layout.addWidget(&title);
    layout.addStretch();
    layout.addWidget(&closeButton);
    layout.addSpacing(16);
}

CustomHead::CustomHead(QWidget* parent)
    : QWidget(parent), icon(this), title("ClipMind", this), layout(this), closeButton(this) {
    setFixedSize(360, 44);
    connect(&closeButton, &QToolButton::clicked, this, &CustomHead::closeRequested);
    setupUI();
}

void CustomHead::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QWidget* topLevelWindow = window();
        if (!topLevelWindow) {
            return;
        }

        if (QWindow* handle = topLevelWindow->windowHandle(); handle && handle->startSystemMove()) {
            event->accept();
            return;
        }

        dragging = true;
        dragPosition = event->globalPosition().toPoint() - topLevelWindow->frameGeometry().topLeft();
        event->accept();
    }
}

void CustomHead::mouseMoveEvent(QMouseEvent* event) {
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        emit moveRequested(event->globalPosition().toPoint() - dragPosition);
        event->accept();
    }
}

void CustomHead::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        event->accept();
    }
}
