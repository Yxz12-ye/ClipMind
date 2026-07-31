#pragma once

#include <QLineEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class QEvent;

class SearchWidget : public QWidget {
    Q_OBJECT
private:
    QLineEdit* lineEdit;
    QWidget* roundedBg;
    QVBoxLayout layout;
    QTimer timer;
    bool m_updatingTheme = false;

    void onTextChanged(const QString& text);
    void applyTheme();

public:
    explicit SearchWidget(QWidget* parent = nullptr);
    ~SearchWidget() = default;

    void focusInput();

signals:
    void inputTextChanged(const QString& text);

protected:
    void changeEvent(QEvent* event) override;
};
