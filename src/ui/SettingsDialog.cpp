#include "SettingsDialog.hpp"

#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPalette>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "CustomHead.hpp"

namespace {

QLabel* createLabel(const QString& text, const QString& objectName, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

QWidget* createSettingRow(const QString& title, const QString& description, QWidget* control,
                          QWidget* parent) {
    auto* row = new QWidget(parent);
    row->setObjectName("settingsRow");
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(16);

    auto* textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(3);
    textLayout->addWidget(createLabel(title, "settingsItemTitle", row));
    textLayout->addWidget(createLabel(description, "settingsItemDescription", row));

    layout->addLayout(textLayout, 1);
    layout->addWidget(control, 0, Qt::AlignVCenter);
    return row;
}

QWidget* createPage(const QString& title, const QString& description, QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(8);
    layout->addWidget(createLabel(title, "settingsPageTitle", page));
    layout->addWidget(createLabel(description, "settingsPageDescription", page));
    layout->addSpacing(16);
    return page;
}

QWidget* createSection(const QString& title, QWidget* parent) {
    auto* section = new QFrame(parent);
    section->setObjectName("settingsSection");
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(createLabel(title, "settingsSectionTitle", section));
    return section;
}

}  // namespace

SettingsDialog::SettingsDialog(bool hideAfterPaste, bool showTrayIcon, QWidget* parent)
    : QDialog(parent), autoHide(nullptr), showInTray(nullptr) {
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setFixedSize(680, 470);
    setupUI();
    autoHide->setChecked(hideAfterPaste);
    showInTray->setChecked(showTrayIcon);
    applyTheme();
}

void SettingsDialog::setupUI() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* panel = new QWidget(this);
    panel->setObjectName("settingsPanel");
    panel->setAttribute(Qt::WA_StyledBackground, true);
    outerLayout->addWidget(panel);

    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    head = new CustomHead(QStringLiteral("设置"), false, panel);
    panelLayout->addWidget(head);

    auto* body = new QWidget(panel);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(16, 4, 16, 16);
    bodyLayout->setSpacing(16);

    categories = new QListWidget(body);
    categories->setObjectName("settingsCategories");
    categories->setFixedWidth(152);
    categories->setFrameShape(QFrame::NoFrame);
    categories->setFocusPolicy(Qt::NoFocus);
    categories->addItems({QStringLiteral("通用"), QStringLiteral("快捷键"), QStringLiteral("外观"),
                          QStringLiteral("关于")});
    categories->setCurrentRow(0);

    pages = new QStackedWidget(body);
    pages->setObjectName("settingsPages");

    auto* generalPage =
        createPage(QStringLiteral("通用"), QStringLiteral("管理窗口和剪贴板的默认行为"), pages);
    auto* generalLayout = qobject_cast<QVBoxLayout*>(generalPage->layout());
    auto* behaviorSection = createSection(QStringLiteral("窗口行为"), generalPage);
    auto* behaviorLayout = qobject_cast<QVBoxLayout*>(behaviorSection->layout());
    autoHide = new QCheckBox(QStringLiteral("自动隐藏"), behaviorSection);
    behaviorLayout->addWidget(createSettingRow(
        QStringLiteral("粘贴后收起窗口"), QStringLiteral("选择剪贴板内容并粘贴后自动隐藏主窗口"),
        autoHide, behaviorSection));
    showInTray = new QCheckBox(QStringLiteral("显示"), behaviorSection);
    behaviorLayout->addWidget(createSettingRow(QStringLiteral("通知区域图标"),
                                               QStringLiteral("在系统通知区域保留 ClipMind 图标"),
                                               showInTray, behaviorSection));
    generalLayout->addWidget(behaviorSection);
    generalLayout->addStretch();

    auto* shortcutPage =
        createPage(QStringLiteral("快捷键"), QStringLiteral("快速呼出 ClipMind"), pages);
    auto* shortcutLayout = qobject_cast<QVBoxLayout*>(shortcutPage->layout());
    auto* shortcutSection = createSection(QStringLiteral("全局快捷键"), shortcutPage);
    auto* shortcutSectionLayout = qobject_cast<QVBoxLayout*>(shortcutSection->layout());
    shortcutSectionLayout->addWidget(createSettingRow(
        QStringLiteral("呼出窗口"), QStringLiteral("当前使用的全局快捷键"),
        createLabel(QStringLiteral("Alt + V"), "settingsShortcutValue", shortcutSection),
        shortcutSection));
    shortcutLayout->addWidget(shortcutSection);
    shortcutLayout->addStretch();

    auto* appearancePage =
        createPage(QStringLiteral("外观"), QStringLiteral("界面随系统主题自动调整"), pages);
    auto* appearanceLayout = qobject_cast<QVBoxLayout*>(appearancePage->layout());
    auto* appearanceSection = createSection(QStringLiteral("主题"), appearancePage);
    auto* appearanceSectionLayout = qobject_cast<QVBoxLayout*>(appearanceSection->layout());
    appearanceSectionLayout->addWidget(createSettingRow(
        QStringLiteral("跟随系统主题"), QStringLiteral("根据系统明暗模式调整界面颜色"),
        createLabel(QStringLiteral("已启用"), "settingsShortcutValue", appearanceSection),
        appearanceSection));
    appearanceLayout->addWidget(appearanceSection);
    appearanceLayout->addStretch();

    auto* aboutPage =
        createPage(QStringLiteral("关于"), QStringLiteral("ClipMind 剪贴板管理器"), pages);
    auto* aboutLayout = qobject_cast<QVBoxLayout*>(aboutPage->layout());
    auto* aboutSection = createSection(QStringLiteral("应用信息"), aboutPage);
    auto* aboutSectionLayout = qobject_cast<QVBoxLayout*>(aboutSection->layout());
    aboutSectionLayout->addWidget(createSettingRow(
        QStringLiteral("版本"), QStringLiteral("当前安装的 ClipMind 版本"),
        createLabel(QStringLiteral("0.1.0"), "settingsShortcutValue", aboutSection), aboutSection));
    aboutLayout->addWidget(aboutSection);
    aboutLayout->addStretch();

    pages->addWidget(generalPage);
    pages->addWidget(shortcutPage);
    pages->addWidget(appearancePage);
    pages->addWidget(aboutPage);

    bodyLayout->addWidget(categories);
    bodyLayout->addWidget(pages, 1);
    panelLayout->addWidget(body, 1);

    connect(head, &CustomHead::closeRequested, this, &QDialog::reject);
    connect(head, &CustomHead::moveRequested, this,
            [this](const QPoint& position) { move(position); });
    connect(categories, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
}

bool SettingsDialog::hideAfterPasteEnabled() const {
    return autoHide->isChecked();
}

bool SettingsDialog::trayIconEnabled() const {
    return showInTray->isChecked();
}

void SettingsDialog::applyTheme() {
    const bool darkMode = palette().color(QPalette::Window).lightness() < 128;
    const QString background = darkMode ? "#1C1C1C" : "#FFFFFF";
    const QString panel = darkMode ? "#252525" : "#F8FAFC";
    const QString border = darkMode ? "#383838" : "#E2E8F0";
    const QString text = darkMode ? "#F1F5F9" : "#0F172A";
    const QString secondaryText = darkMode ? "#94A3B8" : "#64748B";
    const QString hover = darkMode ? "rgba(255, 255, 255, 0.08)" : "rgba(15, 23, 42, 0.06)";

    setStyleSheet(
        QString(
            "QWidget#settingsPanel {"
            "background-color: %1;"
            "border-radius: 12px;"
            "}"
            "QWidget#settingsPanel QToolButton {"
            "border: none; border-radius: 9px; background: transparent;"
            "}"
            "QWidget#settingsPanel QToolButton:hover { background-color: %2; }"
            "QListWidget#settingsCategories {"
            "background: %3; border-radius: 8px; padding: 6px; outline: none;"
            "color: %4;"
            "}"
            "QListWidget#settingsCategories::item {"
            "height: 36px; border-radius: 6px; padding-left: 12px;"
            "}"
            "QListWidget#settingsCategories::item:hover { background: %2; }"
            "QListWidget#settingsCategories::item:selected {"
            "background: #3B82F6; color: white;"
            "}"
            "QStackedWidget#settingsPages { background: transparent; }"
            "QLabel#settingsPageTitle { color: %4; font-size: 20px; font-weight: 600; }"
            "QLabel#settingsPageDescription, QLabel#settingsItemDescription { color: %5; }"
            "QLabel#settingsItemTitle { color: %4; font-weight: 600; }"
            "QLabel#settingsSectionTitle {"
            "color: %5; font-size: 12px; font-weight: 600; padding: 12px 16px 8px 16px;"
            "}"
            "QFrame#settingsSection { background: %3; border: 1px solid %6; border-radius: 8px; }"
            "QFrame#settingsSection QWidget { background: transparent; }"
            "QFrame#settingsSection > QWidget#settingsRow { border-top: 1px solid %6; }"
            "QLabel#settingsShortcutValue { color: #3B82F6; font-weight: 600; }"
            "QCheckBox { color: %4; spacing: 6px; }"
            "QCheckBox::indicator { width: 16px; height: 16px; }"
            "QCheckBox::indicator:unchecked { border: 1px solid %6; border-radius: 4px; }"
            "QCheckBox::indicator:checked { background: #3B82F6; border: 1px solid #3B82F6;"
            "border-radius: 4px; }")
            .arg(background, hover, panel, text, secondaryText, border));
}

void SettingsDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        applyTheme();
    }

    QDialog::changeEvent(event);
}
