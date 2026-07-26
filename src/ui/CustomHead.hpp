#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QSvgWidget>
#include <QToolButton>
#include <QWidget>

class CustomHead : public QWidget {
    Q_OBJECT
private:
    QSvgWidget icon;
    QLabel title;
    QHBoxLayout layout;
    QToolButton settingsButton;
    QToolButton closeButton;

    QPoint dragPosition;
    bool dragging = false;

    void setupUI();

public:
    CustomHead(QWidget* parent);
    CustomHead(const QString& titleText, bool showSettingsButton, QWidget* parent = nullptr);
    ~CustomHead() = default;

signals:
    void closeRequested();
    void settingsRequested();
    void moveRequested(QPoint);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
};
