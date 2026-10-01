#include "SettingEditor.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>

SettingEditor::SettingEditor(QWidget* parent) : QWidget(parent) {}

BoolSettingEditor::BoolSettingEditor(bool value, QWidget* parent) : SettingEditor(parent) {
    checkBox = new QCheckBox(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(checkBox);
    checkBox->setText(QString());
    checkBox->setChecked(value);
    connect(checkBox, &QCheckBox::toggled, this,
            [this](bool checked) { emit valueChanged(checked); });
}

QVariant BoolSettingEditor::value() const {
    return checkBox->isChecked();
}

void BoolSettingEditor::setValue(const QVariant& value) {
    const QSignalBlocker blocker(checkBox);
    checkBox->setChecked(value.toBool());
}

StringSettingEditor::StringSettingEditor(const QString& value, QWidget* parent)
    : SettingEditor(parent) {
    lineEdit = new QLineEdit(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(lineEdit);
    lineEdit->setText(value);
    lineEdit->setMinimumWidth(180);
    connect(lineEdit, &QLineEdit::textChanged, this,
            [this](const QString& text) { emit valueChanged(text); });
}

QVariant StringSettingEditor::value() const {
    return lineEdit->text();
}

void StringSettingEditor::setValue(const QVariant& value) {
    const QSignalBlocker blocker(lineEdit);
    lineEdit->setText(value.toString());
}

IntSettingEditor::IntSettingEditor(int value, QWidget* parent) : SettingEditor(parent) {
    spinBox = new QSpinBox(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(spinBox);
    spinBox->setRange(-999999999, 999999999);
    spinBox->setValue(value);
    connect(spinBox, &QSpinBox::valueChanged, this,
            [this](int number) { emit valueChanged(number); });
}

QVariant IntSettingEditor::value() const {
    return spinBox->value();
}

void IntSettingEditor::setValue(const QVariant& value) {
    const QSignalBlocker blocker(spinBox);
    spinBox->setValue(value.toInt());
}

EnumSettingEditor::EnumSettingEditor(const QVector<QPair<QString, QString>>& options,
                                     const QString& value, QWidget* parent)
    : SettingEditor(parent) {
    comboBox = new QComboBox(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(comboBox);
    for (const auto& option : options) {
        comboBox->addItem(option.second, option.first);
    }
    setValue(value);
    connect(comboBox, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { emit valueChanged(this->value()); });
}

QVariant EnumSettingEditor::value() const {
    return comboBox->currentData();
}

void EnumSettingEditor::setValue(const QVariant& value) {
    const QSignalBlocker blocker(comboBox);
    const int index = comboBox->findData(value.toString());
    if (index >= 0) {
        comboBox->setCurrentIndex(index);
    }
}

FixedTextSettingEditor::FixedTextSettingEditor(const QString& value, QWidget* parent)
    : SettingEditor(parent) {
    label = new QLabel(value, this);
    label->setObjectName("settingsFixedValue");
    label->setWordWrap(true);
    // 只读但不是"死"的: 允许选中复制, 比如把版本号贴到别处
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(label);
}

QVariant FixedTextSettingEditor::value() const {
    return label->text();
}

void FixedTextSettingEditor::setValue(const QVariant& value) {
    label->setText(value.toString());
}
