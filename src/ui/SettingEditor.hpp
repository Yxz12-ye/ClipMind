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
