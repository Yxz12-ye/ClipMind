#pragma once

#include <QLineEdit>
#include <QVBoxLayout>

class SearchWidget : public QWidget
{
    Q_OBJECT
private:
    QLineEdit* lineEdit;
    QVBoxLayout layout;


public:
    explicit SearchWidget(QWidget* parent = nullptr);
    ~SearchWidget()=default;

    void focusInput();
};
