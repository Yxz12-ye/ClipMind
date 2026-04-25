#pragma once

#include <QWidget>
#include <QSvgWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QToolButton>
#include <QPoint>
#include <QMouseEvent>

class CustomHead:public QWidget
{
    Q_OBJECT
private:
    QSvgWidget icon;
    QLabel title; 
    QHBoxLayout layout;
    QToolButton closeButton;

    QPoint dragPosition;
    bool dragging = false;

    void setupUI();

public:
    CustomHead(QWidget* parent);
    ~CustomHead()=default;

signals:
    void closeRequested();
    void moveRequested(QPoint);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
};
