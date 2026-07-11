#pragma once

#include <QLineEdit>
#include <QVBoxLayout>
#include <QTimer>

class SearchWidget : public QWidget
{
    Q_OBJECT
private:
    QLineEdit* lineEdit;
    QVBoxLayout layout;
    QTimer timer;

    void onTextChanged(const QString& text);

public:
    explicit SearchWidget(QWidget* parent = nullptr);
    ~SearchWidget()=default;

    void focusInput();

signals:
    void inputTextChanged(const QString& text);
};
