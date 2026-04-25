#include "MainWindow.hpp"

#include <QEvent>
#include <QPalette>

void MainWindow::setupUI() {
    layout.setContentsMargins(QMargins(0,0,0,0));
    layout.addWidget(&head);
    layout.addStretch();
}

void MainWindow::applyTheme() {
    const bool darkMode = palette().color(QPalette::Window).lightness() < 128;
    const QString background = darkMode ? "#1C1C1C" : "#F7F7F8";
    const QString foreground = darkMode ? "#F5F5F5" : "#111827";
    const QString hover = darkMode ? "rgba(255, 255, 255, 0.08)" : "rgba(15, 23, 42, 0.06)";

    central.setStyleSheet(QString(
        "QWidget#centralPanel {"
        "background-color: %1;"
        "border-radius: 12px;"
        "}"
        "QWidget#centralPanel QLabel {"
        "color: %2;"
        "}"
        "QWidget#centralPanel QToolButton {"
        "border: none;"
        "border-radius: 9px;"
        "background: transparent;"
        "}"
        "QWidget#centralPanel QToolButton:hover {"
        "background-color: %3;"
        "}"
    ).arg(background, foreground, hover));
}

MainWindow::MainWindow() : central(this), head(&central), layout(&central) {
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(360, 400);
    central.setObjectName("centralPanel");
    central.setAttribute(Qt::WA_StyledBackground, true);
    applyTheme();
    setCentralWidget(&central);
    connect(&head, &CustomHead::closeRequested, this, &QWidget::close);
    connect(&head, &CustomHead::moveRequested, this, [=](QPoint pos){move(pos);});
    setupUI();
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        applyTheme();
    }

    QMainWindow::changeEvent(event);
}
