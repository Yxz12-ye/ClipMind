#include "SettingsDialog.hpp"

#include <QBrush>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include "CustomHead.hpp"
#include "struct.hpp"

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

QString modeName(SearchMode mode) {
    switch (mode) {
    case SearchMode::Semantics:
        return QStringLiteral("Semantics");
    case SearchMode::Regex:
        return QStringLiteral("Regex");
    case SearchMode::None:
        return QStringLiteral("None");
    }

    return QString();
}

class TagManagerDelegate : public QStyledItemDelegate {
public:
    explicit TagManagerDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        if (option->state & QStyle::State_Selected) {
            const QVariant foreground = index.data(Qt::ForegroundRole);
            if (foreground.canConvert<QBrush>()) {
                option->palette.setBrush(QPalette::HighlightedText,
                                         qvariant_cast<QBrush>(foreground));
            }
        }
    }
};

class TagEditorDialog : public QDialog {
public:
    explicit TagEditorDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(QStringLiteral("添加标签"));
        setModal(true);
        setFixedSize(440, 350);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 20, 24, 20);
        layout->setSpacing(10);

        auto* title = new QLabel(QStringLiteral("添加标签"), this);
        title->setObjectName("tagEditorTitle");
        layout->addWidget(title);

        layout->addWidget(new QLabel(QStringLiteral("名称"), this));
        nameInput = new QLineEdit(this);
        nameInput->setPlaceholderText(QStringLiteral("例如：链接"));
        layout->addWidget(nameInput);

        layout->addWidget(new QLabel(QStringLiteral("匹配方式"), this));
        modeInput = new QComboBox(this);
        modeInput->addItem(QStringLiteral("Semantics"), static_cast<int>(SearchMode::Semantics));
        modeInput->addItem(QStringLiteral("Regex"), static_cast<int>(SearchMode::Regex));
        modeInput->addItem(QStringLiteral("None"), static_cast<int>(SearchMode::None));
        layout->addWidget(modeInput);

        ruleLabel = new QLabel(QStringLiteral("规则（用于语义或正则匹配）"), this);
        layout->addWidget(ruleLabel);
        ruleInput = new QLineEdit(this);
        ruleInput->setPlaceholderText(QStringLiteral("例如：网页链接 或 https?://\\S+"));
        layout->addWidget(ruleInput);

        auto* colorLayout = new QHBoxLayout;
        colorLayout->setContentsMargins(0, 2, 0, 2);
        colorLayout->setSpacing(10);
        colorLayout->addWidget(new QLabel(QStringLiteral("文本颜色"), this));
        foregroundButton = new QPushButton(this);
        foregroundButton->setObjectName("tagColorButton");
        foregroundButton->setToolTip(QStringLiteral("选择标签文本颜色"));
        foregroundButton->setFixedSize(28, 28);
        colorLayout->addWidget(foregroundButton);
        colorLayout->addSpacing(12);
        colorLayout->addWidget(new QLabel(QStringLiteral("背景颜色"), this));
        backgroundButton = new QPushButton(this);
        backgroundButton->setObjectName("tagColorButton");
        backgroundButton->setToolTip(QStringLiteral("选择标签背景颜色"));
        backgroundButton->setFixedSize(28, 28);
        colorLayout->addWidget(backgroundButton);
        colorLayout->addStretch();
        layout->addLayout(colorLayout);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("添加"));
        buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
        layout->addWidget(buttons);

        updateColorButton(foregroundButton, foregroundColor);
        updateColorButton(backgroundButton, backgroundColor);
        updateRuleHint();
        applyTheme();
        connect(nameInput, &QLineEdit::textChanged, this, [buttons](const QString& value) {
            buttons->button(QDialogButtonBox::Ok)->setEnabled(!value.trimmed().isEmpty());
        });
        connect(modeInput, &QComboBox::currentIndexChanged, this,
                [this](int) { updateRuleHint(); });
        connect(foregroundButton, &QPushButton::clicked, this,
                [this] { selectColor(foregroundButton, foregroundColor); });
        connect(backgroundButton, &QPushButton::clicked, this,
                [this] { selectColor(backgroundButton, backgroundColor); });
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    Tag tag() const {
        const auto mode = static_cast<SearchMode>(modeInput->currentData().toInt());
        return Tag(nameInput->text().trimmed(), ruleInput->text().trimmed(), mode, backgroundColor,
                   foregroundColor);
    }

private:
    QLineEdit* nameInput;
    QComboBox* modeInput;
    QLabel* ruleLabel;
    QLineEdit* ruleInput;
    QPushButton* foregroundButton;
    QPushButton* backgroundButton;
    QColor foregroundColor = QColor("#1E3A8A");
    QColor backgroundColor = QColor("#DBEAFE");

    void applyTheme() {
        const bool darkMode = palette().color(QPalette::Window).lightness() < 128;
        const QString background = darkMode ? "#1C1C1C" : "#FFFFFF";
        const QString border = darkMode ? "#383838" : "#CBD5E1";
        const QString text = darkMode ? "#F1F5F9" : "#0F172A";
        const QString inputBackground = darkMode ? "#252525" : "#F8FAFC";

        setStyleSheet(
            QString(
                "QDialog { background: %1; color: %2; }"
                "QLabel { color: %2; }"
                "QLabel#tagEditorTitle { font-size: 18px; font-weight: 600; }"
                "QLineEdit, QComboBox {"
                "background: %3; border: 1px solid %4; border-radius: 6px; padding: 6px 8px;"
                "color: %2;"
                "}"
                "QPushButton { border-radius: 6px; padding: 6px 12px; }"
                "QDialogButtonBox QPushButton { background: #3B82F6; border: none; color: white; }"
                "QDialogButtonBox QPushButton:hover { background: #2563EB; }")
                .arg(background, text, inputBackground, border));
    }

    void updateColorButton(QPushButton* button, const QColor& color) {
        button->setStyleSheet(
            QString("background-color: %1; border: 1px solid #94A3B8; border-radius: 5px;")
                .arg(color.name(QColor::HexRgb)));
    }

    void selectColor(QPushButton* button, QColor& color) {
        const QColor selectedColor =
            QColorDialog::getColor(color, this, QStringLiteral("选择颜色"));
        if (!selectedColor.isValid()) {
            return;
        }

        color = selectedColor;
        updateColorButton(button, color);
    }

    void updateRuleHint() {
        const auto mode = static_cast<SearchMode>(modeInput->currentData().toInt());
        if (mode == SearchMode::Semantics) {
            ruleLabel->setText(QStringLiteral("语义规则（用于自动语义匹配）"));
        } else if (mode == SearchMode::Regex) {
            ruleLabel->setText(QStringLiteral("正则表达式（用于自动正则匹配）"));
        } else {
            ruleLabel->setText(QStringLiteral("规则（None 暂不参与自动匹配）"));
        }
    }
};

enum TagDataRole {
    TagNameRole = Qt::UserRole,
    TagRuleRole,
    TagModeRole,
    TagForegroundRole,
    TagBackgroundRole,
    TagSystemRole,
};

}  // namespace

SettingsDialog::SettingsDialog(bool hideAfterPaste, bool showTrayIcon, QWidget* parent)
    : QDialog(parent), autoHide(nullptr), showInTray(nullptr) {
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setFixedSize(760, 520);
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
    categories->addItems({QStringLiteral("通用"), QStringLiteral("标签管理"),
                          QStringLiteral("快捷键"), QStringLiteral("外观"),
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

    auto* tagPage =
        createPage(QStringLiteral("标签管理"),
                   QStringLiteral("通过上下按钮调整自动匹配优先级，越靠上优先级越高"), pages);
    auto* tagLayout = qobject_cast<QVBoxLayout*>(tagPage->layout());
    auto* tagToolbar = new QHBoxLayout;
    tagToolbar->setContentsMargins(0, 0, 0, 0);
    auto* priorityHint = createLabel(QStringLiteral("单条内容仅应用优先级最高的一个自动匹配标签"),
                                     "settingsItemDescription", tagPage);
    auto* addTagButton = new QPushButton(QStringLiteral("+ 添加标签"), tagPage);
    addTagButton->setObjectName("addTagButton");
    auto* moveUpButton = new QToolButton(tagPage);
    moveUpButton->setObjectName("tagToolbarButton");
    moveUpButton->setArrowType(Qt::UpArrow);
    moveUpButton->setToolTip(QStringLiteral("上移标签，提高优先级"));
    auto* moveDownButton = new QToolButton(tagPage);
    moveDownButton->setObjectName("tagToolbarButton");
    moveDownButton->setArrowType(Qt::DownArrow);
    moveDownButton->setToolTip(QStringLiteral("下移标签，降低优先级"));
    auto* deleteTagButton = new QPushButton(QStringLiteral("删除"), tagPage);
    deleteTagButton->setObjectName("deleteTagButton");
    deleteTagButton->setEnabled(false);
    tagToolbar->addWidget(priorityHint, 1);
    tagToolbar->addWidget(addTagButton);
    tagToolbar->addWidget(moveUpButton);
    tagToolbar->addWidget(moveDownButton);
    tagToolbar->addWidget(deleteTagButton);
    tagLayout->addLayout(tagToolbar);

    tagList = new QListWidget(tagPage);
    tagList->setObjectName("tagManagerList");
    tagList->setItemDelegate(new TagManagerDelegate(tagList));
    tagList->setFrameShape(QFrame::NoFrame);
    tagList->setSelectionMode(QAbstractItemView::SingleSelection);
    tagList->setDragDropMode(QAbstractItemView::NoDragDrop);
    tagList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    tagLayout->addWidget(tagList, 1);

    addTagItem(Tag(QStringLiteral("LINK"), QStringLiteral("https?://\\S+"), SearchMode::Regex,
                   QColor("#DBEAFE"), QColor("#1D4ED8"), true));

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
    pages->addWidget(tagPage);
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
    connect(addTagButton, &QPushButton::clicked, this, [this] { addTag(); });
    connect(moveUpButton, &QToolButton::clicked, this,
            [this] { moveTagItem(tagList->currentItem(), -1); });
    connect(moveDownButton, &QToolButton::clicked, this,
            [this] { moveTagItem(tagList->currentItem(), 1); });
    connect(deleteTagButton, &QPushButton::clicked, this,
            [this] { removeTagItem(tagList->currentItem()); });
    connect(tagList, &QListWidget::currentItemChanged, this,
            [deleteTagButton](QListWidgetItem* current, QListWidgetItem*) {
                deleteTagButton->setEnabled(current != nullptr &&
                                            !current->data(TagSystemRole).toBool());
            });
}

void SettingsDialog::addTag() {
    TagEditorDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    addTagItem(dialog.tag());
}

void SettingsDialog::addTagItem(const Tag& tag) {
    auto* item = new QListWidgetItem(tagList);
    item->setData(TagNameRole, tag.tagName);
    item->setData(TagRuleRole, tag.rule);
    item->setData(TagModeRole, static_cast<int>(tag.mode));
    item->setData(TagForegroundRole, tag.tagNameColor);
    item->setData(TagBackgroundRole, tag.tagBackColor);
    item->setData(TagSystemRole, tag.isSysTag);
    item->setText(QStringLiteral("%1    %2  |  %3%4")
                      .arg(tag.tagName, modeName(tag.mode), tag.rule,
                           tag.isSysTag ? QStringLiteral("    系统") : QString()));
    item->setToolTip(QStringLiteral("%1\n%2").arg(modeName(tag.mode), tag.rule));
    item->setForeground(QBrush(tag.tagNameColor));
    item->setBackground(QBrush(tag.tagBackColor));
    item->setSizeHint(QSize(0, 34));
    tagList->setCurrentItem(item);
}

void SettingsDialog::removeTagItem(QListWidgetItem* item) {
    if (item == nullptr || item->data(TagSystemRole).toBool()) {
        return;
    }

    delete tagList->takeItem(tagList->row(item));
}

void SettingsDialog::moveTagItem(QListWidgetItem* item, int offset) {
    if (item == nullptr) {
        return;
    }

    const int currentRow = tagList->row(item);
    const int targetRow = qBound(0, currentRow + offset, tagList->count() - 1);
    if (currentRow == targetRow) {
        return;
    }

    QListWidgetItem* movedItem = tagList->takeItem(currentRow);
    tagList->insertItem(targetRow, movedItem);
    tagList->setCurrentItem(movedItem);
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
            "QListWidget#tagManagerList { background: transparent; outline: none; }"
            "QListWidget#tagManagerList::item { border: 1px solid %6; border-radius: 6px; "
            "margin-bottom: 4px; padding: 5px 8px; }"
            "QListWidget#tagManagerList::item:selected { border: 2px solid #3B82F6; }"
            "QToolButton#tagToolbarButton { border: none; border-radius: 6px; color: %5; }"
            "QToolButton#tagToolbarButton:hover { background: %2; color: %4; }"
            "QPushButton#addTagButton {"
            "background: #3B82F6; border: none; border-radius: 6px; color: white; padding: 6px "
            "12px;"
            "}"
            "QPushButton#addTagButton:hover { background: #2563EB; }"
            "QPushButton#deleteTagButton {"
            "background: transparent; border: none; color: #DC2626; padding: 4px 6px;"
            "}"
            "QPushButton#deleteTagButton:hover { background: rgba(220, 38, 38, 0.10); "
            "border-radius: 5px; }"
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
