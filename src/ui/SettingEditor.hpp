#pragma once

#include <QPair>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class SettingEditor : public QWidget {
    Q_OBJECT

public:
    explicit SettingEditor(QWidget* parent = nullptr);
    ~SettingEditor() override = default;

    virtual QVariant value() const = 0;
    virtual void setValue(const QVariant& value) = 0;

signals:
    void valueChanged(const QVariant& value);
};

class BoolSettingEditor final : public SettingEditor {
    Q_OBJECT

public:
    explicit BoolSettingEditor(bool value, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValue(const QVariant& value) override;

private:
    QCheckBox* checkBox;
};

class StringSettingEditor final : public SettingEditor {
    Q_OBJECT

public:
    explicit StringSettingEditor(const QString& value, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValue(const QVariant& value) override;

private:
    QLineEdit* lineEdit;
};

class IntSettingEditor final : public SettingEditor {
    Q_OBJECT

public:
    explicit IntSettingEditor(int value, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValue(const QVariant& value) override;

private:
    QSpinBox* spinBox;
};

class EnumSettingEditor final : public SettingEditor {
    Q_OBJECT

public:
    EnumSettingEditor(const QVector<QPair<QString, QString>>& options, const QString& value,
                      QWidget* parent = nullptr);
    QVariant value() const override;
    void setValue(const QVariant& value) override;

private:
    QComboBox* comboBox;
};

// 固定文本: 只读展示一段文本(软件版本号这类信息), 用户改不了, 因此不发 valueChanged
class FixedTextSettingEditor final : public SettingEditor {
    Q_OBJECT

public:
    explicit FixedTextSettingEditor(const QString& value, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValue(const QVariant& value) override;

private:
    QLabel* label;
};

// 动作项结果提示的语义: 决定提示文字用什么颜色
enum class SettingActionState {
    Pending,  // 已触发, 结果还没回来
    Success,
    Failure,
};

// 动作项: 一个按钮加一行结果提示, 自身没有取值
// 点击只发出 triggered(), 具体做什么、结果如何都由外部决定, 再通过 setStatus() 推回来
class ActionSettingEditor final : public SettingEditor {
    Q_OBJECT

public:
    explicit ActionSettingEditor(const QString& text, QWidget* parent = nullptr);
    // 动作项没有取值: value() 返回的是当前结果提示文本
    QVariant value() const override;
    // 更新结果提示文本(不带颜色, 也就是 Pending 状态)
    void setValue(const QVariant& value) override;
    // 更新结果提示文本并按下 state 上色
    void setStatus(const QString& text, SettingActionState state);
    // 请求进行中禁用按钮, 避免重复触发
    void setBusy(bool busy);

signals:
    void triggered();

private:
    QPushButton* button;
    QLabel* status;
};
