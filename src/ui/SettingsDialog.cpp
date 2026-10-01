#include "SettingsDialog.hpp"

#include <QBrush>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include "CustomHead.hpp"
#include "SettingEditor.hpp"
#include "service/SettingRegistry.hpp"
#include "struct.hpp"

namespace {

QLabel* createLabel(const QString& text, const QString& objectName, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
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
    explicit TagEditorDialog(QWidget* parent = nullptr) : TagEditorDialog(Tag(), parent) {}
    explicit TagEditorDialog(const Tag& tag, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(tag.tagName.isEmpty() ? QStringLiteral("添加标签")
                                             : QStringLiteral("编辑标签"));
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
        modeInput->setCurrentIndex(modeInput->findData(static_cast<int>(SearchMode::Regex)));
        layout->addWidget(modeInput);

        ruleLabel = new QLabel(QStringLiteral("匹配规则"), this);
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
        buttons->button(QDialogButtonBox::Ok)
            ->setText(tag.tagName.isEmpty() ? QStringLiteral("添加") : QStringLiteral("保存"));
        buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!tag.tagName.isEmpty());
        layout->addWidget(buttons);

        if (!tag.tagName.isEmpty()) {
            nameInput->setText(tag.tagName);
            const int modeIndex = modeInput->findData(static_cast<int>(tag.mode));
            if (modeIndex >= 0) {
                modeInput->setCurrentIndex(modeIndex);
            }
            ruleInput->setText(tag.rule);
            foregroundColor = tag.tagNameColor;
            backgroundColor = tag.tagBackColor;
        }

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
            ruleLabel->setText(QStringLiteral("语义匹配规则（暂未启用）"));
        } else if (mode == SearchMode::Regex) {
            ruleLabel->setText(QStringLiteral("正则表达式"));
        } else {
            ruleLabel->setText(QStringLiteral("规则（不参与匹配）"));
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

// 标签页不受注册服务约束, 这个 id 只用于跳过注册表里的同名页与生成 objectName
const QString kTagPageId = QStringLiteral("core/tags");
const QString kTagPageTitle = QStringLiteral("标签管理");
const QString kTagPageBrief = QStringLiteral("标签显示与自动匹配顺序");

}  // namespace

namespace WidgetFactory {

QVBoxLayout* contentLayout(QWidget* container) {
    return container != nullptr ? qobject_cast<QVBoxLayout*>(container->layout()) : nullptr;
}

QWidget* createPage(const QString& pageId, QWidget* parent, const QString& title,
                    const QString& brief) {
    auto* page = new QWidget(parent);
    // `/` 在样式表选择器里要转义, 统一换成 `-`, 页面样式可以写成 #settingsPage-core-tags
    QString idSuffix = pageId;
    idSuffix.replace(QLatin1Char('/'), QLatin1Char('-'));
    page->setObjectName(QStringLiteral("settingsPage-") + idSuffix);

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(8);
    layout->addWidget(createLabel(title, "settingsPageTitle", page));
    layout->addWidget(createLabel(brief, "settingsPageDescription", page));
    layout->addSpacing(16);
    return page;
}

QWidget* createGroup(const QString& groupId, QWidget* parent, const QString& title) {
    // 样式表按 objectName=settingsSection 配色, 所以分组 id 只能挂到动态属性上
    auto* group = new QFrame(parent);
    group->setObjectName("settingsSection");
    group->setProperty("groupId", groupId);

    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(createLabel(title, "settingsSectionTitle", group));
    return group;
}

QWidget* createRow(const QString& title, const QString& description, QWidget* control,
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

}  // namespace WidgetFactory

SettingsDialog::SettingsDialog(SettingRegistry* registry, QWidget* parent)
    : QDialog(parent), registry(registry) {
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setFixedSize(760, 520);
    setupUI();
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
    // 自定义窗口头
    head = new CustomHead(QStringLiteral("设置"), false, panel);
    panelLayout->addWidget(head);
    // 设置容器及横向布局
    auto* body = new QWidget(panel);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(16, 4, 16, 16);
    bodyLayout->setSpacing(16);
    // 左侧Tab
    categories = new QListWidget(body);
    categories->setObjectName("settingsCategories");
    categories->setFixedWidth(152);
    categories->setFrameShape(QFrame::NoFrame);
    categories->setFocusPolicy(Qt::NoFocus);
    // 堆叠具体的设置页面
    pages = new QStackedWidget(body);
    pages->setObjectName("settingsPages");

    // 无需根据pageId去细分, 只要把页面文本交给工厂即可, 除了Tag标签, 其余都走同一套逻辑
    bool tagPageCreated = false;
    if (registry != nullptr) {
        for (const SettingPageDefinition& definition : registry->pages()) {
            if (definition.id == kTagPageId) {
                // 标签页自绘(标签数据由外部推送), 只沿用注册表里的标题文案
                addCategory(definition.title,
                            createTagPage(definition.title, definition.description));
                tagPageCreated = true;
                continue;
            }
            addCategory(definition.title, createStandardPage(definition));
        }
    }
    if (!tagPageCreated) {
        // 注册表没有登记标签页时, 也要保留标签管理入口
        addCategory(kTagPageTitle, createTagPage(kTagPageTitle, kTagPageBrief));
    }
    if (categories->count() > 0) {
        categories->setCurrentRow(0);
    }

    bodyLayout->addWidget(categories);
    bodyLayout->addWidget(pages, 1);
    panelLayout->addWidget(body, 1);

    connect(head, &CustomHead::closeRequested, this, &QDialog::reject);
    connect(head, &CustomHead::moveRequested, this,
            [this](const QPoint& position) { move(position); });
    connect(categories, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    // 显示值同步由 Controller 负责: SettingService::valueChanged -> setSettingValue()
    // 本类不持有 SettingService, 只等着被推值
}

void SettingsDialog::addCategory(const QString& title, QWidget* page) {
    categories->addItem(title);
    pages->addWidget(page);
}

QWidget* SettingsDialog::createStandardPage(const SettingPageDefinition& definition) {
    QWidget* page =
        WidgetFactory::createPage(definition.id, pages, definition.title, definition.description);
    QVBoxLayout* pageLayout = WidgetFactory::contentLayout(page);
    for (const SettingGroupDefinition& group : registry->groups(definition.id)) {
        QWidget* section = WidgetFactory::createGroup(group.id, page, group.title);
        QVBoxLayout* sectionLayout = WidgetFactory::contentLayout(section);
        for (const SettingDefinition& setting : registry->settings(group.id)) {
            SettingEditor* editor = createEditor(setting, section);
            if (editor == nullptr) {
                continue;  // 不认识的类型直接跳过该行
            }
            sectionLayout->addWidget(
                WidgetFactory::createRow(setting.title, setting.description, editor, section));
            bindEditor(setting, editor);
        }
        pageLayout->addWidget(section);
    }
    pageLayout->addStretch();
    return page;
}

QWidget* SettingsDialog::createTagPage(const QString& title, const QString& brief) {
    QWidget* page = WidgetFactory::createPage(kTagPageId, pages, title, brief);
    QVBoxLayout* pageLayout = WidgetFactory::contentLayout(page);

    auto* tagToolbar = new QHBoxLayout;
    tagToolbar->setContentsMargins(0, 0, 0, 0);
    auto* systemTagHint =
        createLabel(QStringLiteral("系统保留标签不可编辑或删除"), "settingsItemDescription", page);
    auto* addTagButton = new QPushButton(QStringLiteral("+ 添加标签"), page);
    addTagButton->setObjectName("addTagButton");
    auto* moveUpButton = new QToolButton(page);
    moveUpButton->setObjectName("tagToolbarButton");
    moveUpButton->setArrowType(Qt::UpArrow);
    moveUpButton->setToolTip(QStringLiteral("上移标签"));
    auto* moveDownButton = new QToolButton(page);
    moveDownButton->setObjectName("tagToolbarButton");
    moveDownButton->setArrowType(Qt::DownArrow);
    moveDownButton->setToolTip(QStringLiteral("下移标签"));
    auto* deleteTagButton = new QPushButton(QStringLiteral("删除"), page);
    deleteTagButton->setObjectName("deleteTagButton");
    deleteTagButton->setEnabled(false);
    tagToolbar->addWidget(systemTagHint, 1);
    tagToolbar->addWidget(addTagButton);
    tagToolbar->addWidget(moveUpButton);
    tagToolbar->addWidget(moveDownButton);
    tagToolbar->addWidget(deleteTagButton);
    pageLayout->addLayout(tagToolbar);

    tagList = new QListWidget(page);
    tagList->setObjectName("tagManagerList");
    tagList->setItemDelegate(new TagManagerDelegate(tagList));
    tagList->setFrameShape(QFrame::NoFrame);
    tagList->setSelectionMode(QAbstractItemView::SingleSelection);
    tagList->setDragDropMode(QAbstractItemView::NoDragDrop);
    tagList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    pageLayout->addWidget(tagList, 1);

    // 所有交互都只发信号, 列表内容等 Controller 调 setTags() 回推
    connect(addTagButton, &QPushButton::clicked, this, &SettingsDialog::requestTagAdd);
    connect(moveUpButton, &QToolButton::clicked, this,
            [this] { requestTagMove(tagList->currentItem(), -1); });
    connect(moveDownButton, &QToolButton::clicked, this,
            [this] { requestTagMove(tagList->currentItem(), 1); });
    connect(deleteTagButton, &QPushButton::clicked, this,
            [this] { requestTagDelete(tagList->currentItem()); });
    connect(tagList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) { requestTagEdit(item); });
    connect(tagList, &QListWidget::currentItemChanged, this,
            [deleteTagButton](QListWidgetItem* current, QListWidgetItem*) {
                deleteTagButton->setEnabled(current != nullptr &&
                                            !current->data(TagSystemRole).toBool());
            });
    return page;
}

SettingEditor* SettingsDialog::createEditor(const SettingDefinition& setting, QWidget* parent) {
    // 先用注册时的默认值把控件建出来, Controller 会在显示前用 setSettingValues() 覆盖成当前值
    const QVariant currentValue = setting.defaultValue;
    switch (setting.type) {
    case SettingType::Boolean:
        return new BoolSettingEditor(currentValue.toBool(), parent);
    case SettingType::String:
        return new StringSettingEditor(currentValue.toString(), parent);
    case SettingType::Integer:
        return new IntSettingEditor(currentValue.toInt(), parent);
    case SettingType::Enum: {
        QVector<QPair<QString, QString>> options;
        for (const auto& option : setting.options) {
            options.append(qMakePair(option.value, option.label));
        }
        return new EnumSettingEditor(options, currentValue.toString(), parent);
    }
    case SettingType::FixedText:
        // 固定文本项只读, 文本在注册时就定好了, 之后不会再变
        return new FixedTextSettingEditor(currentValue.toString(), parent);
    case SettingType::Action:
        // 动作项只是一个按钮, defaultValue 里存的是按钮文本
        return new ActionSettingEditor(currentValue.toString(), parent);
    }
    return nullptr;
}

void SettingsDialog::bindEditor(const SettingDefinition& setting, SettingEditor* editor) {
    if (setting.type == SettingType::FixedText) {
        // 固定文本项没有可写取值: 文本由注册表给定, 也不需要接收外部的取值推送,
        // 因此不进 editors 表(编辑控件本身也不会发 valueChanged)
        return;
    }

    // 动作项入表是为了能通过 setActionStatus() 推回结果提示, 但它没有取值, 只转发点击
    editors.insert(setting.key, editor);
    if (auto* actionEditor = qobject_cast<ActionSettingEditor*>(editor); actionEditor != nullptr) {
        connect(actionEditor, &ActionSettingEditor::triggered, this,
                [this, key = setting.key] { emit actionTriggered(key); });
        return;
    }

    connect(editor, &SettingEditor::valueChanged, this,
            [this, key = setting.key](const QVariant& value) {
                // 不再通过registry直接更改键值, 而是抛给Controller去改
                // (本质上View层就不应该直接去改变值, 只要负责好界面和事件就行)
                emit valueChanged(key, value);
            });
}

void SettingsDialog::setActionStatus(const QString& settingId, const QString& text,
                                     SettingActionState state) {
    if (auto* actionEditor = qobject_cast<ActionSettingEditor*>(editors.value(settingId, nullptr));
        actionEditor != nullptr) {
        actionEditor->setStatus(text, state);
        actionEditor->setBusy(state == SettingActionState::Pending);
    }
}

void SettingsDialog::setSettingValue(const QString& settingId, const QVariant& value) {
    // SettingEditor::setValue() 内部屏蔽了信号, 所以这里不会回发 valueChanged
    if (SettingEditor* editor = editors.value(settingId, nullptr); editor != nullptr) {
        editor->setValue(value);
    }
}

void SettingsDialog::setSettingValues(const QHash<QString, QVariant>& values) {
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        setSettingValue(it.key(), it.value());
    }
}

void SettingsDialog::setTags(const QVector<Tag>& tags) {
    if (tagList == nullptr) {
        return;
    }

    // 尽量保住当前选中项, 方便连续编辑
    const QString selectedName = tagList->currentItem() != nullptr
                                     ? tagList->currentItem()->data(TagNameRole).toString()
                                     : QString();

    tagList->clear();
    for (const Tag& tag : tags) {
        addTagItem(tag);
    }

    if (!selectedName.isEmpty()) {
        for (int row = 0; row < tagList->count(); ++row) {
            if (tagList->item(row)->data(TagNameRole).toString() == selectedName) {
                tagList->setCurrentRow(row);
                break;
            }
        }
    }
}

void SettingsDialog::showTagError(const QString& title, const QString& message) {
    QMessageBox::warning(this, title, message);
}

void SettingsDialog::addTagItem(const Tag& tag) {
    auto* item = new QListWidgetItem(tagList);
    updateTagItem(item, tag);
}

void SettingsDialog::updateTagItem(QListWidgetItem* item, const Tag& tag) {
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
}

Tag SettingsDialog::tagFromItem(QListWidgetItem* item) const {
    return Tag(item->data(TagNameRole).toString(), item->data(TagRuleRole).toString(),
               static_cast<SearchMode>(item->data(TagModeRole).toInt()),
               item->data(TagBackgroundRole).value<QColor>(),
               item->data(TagForegroundRole).value<QColor>(), item->data(TagSystemRole).toBool());
}

void SettingsDialog::requestTagAdd() {
    TagEditorDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // 重名、落库失败都交给 Controller 判断, 需要提示时调用 showTagError()
    emit tagAddRequested(dialog.tag());
}

void SettingsDialog::requestTagEdit(QListWidgetItem* item) {
    if (item == nullptr || item->data(TagSystemRole).toBool()) {
        return;
    }

    const Tag original = tagFromItem(item);
    TagEditorDialog dialog(original, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    emit tagUpdateRequested(original.tagName, dialog.tag());
}

void SettingsDialog::requestTagDelete(QListWidgetItem* item) {
    if (item == nullptr || item->data(TagSystemRole).toBool()) {
        return;
    }

    emit tagDeleteRequested(item->data(TagNameRole).toString());
}

void SettingsDialog::requestTagMove(QListWidgetItem* item, int offset) {
    if (item == nullptr) {
        return;
    }

    const int currentRow = tagList->row(item);
    const int targetRow = qBound(0, currentRow + offset, tagList->count() - 1);
    if (currentRow == targetRow) {
        return;
    }

    // 只上报用户期望的顺序, 界面上不动手; Controller 存好后用 setTags() 回推, 失败则原样推回
    QStringList orderedNames;
    orderedNames.reserve(tagList->count());
    for (int row = 0; row < tagList->count(); ++row) {
        orderedNames.append(tagList->item(row)->data(TagNameRole).toString());
    }
    orderedNames.move(currentRow, targetRow);
    emit tagReorderRequested(orderedNames);
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
            "QFrame#settingsSection QLineEdit, QFrame#settingsSection QComboBox {"
            "background: %1; border: 1px solid %6; border-radius: 6px; padding: 6px 8px;"
            "color: %4;"
            "}"
            "QPushButton#settingActionButton {"
            "background: #3B82F6; border: none; border-radius: 6px; color: white; padding: 6px "
            "12px;"
            "}"
            "QPushButton#settingActionButton:hover { background: #2563EB; }"
            "QPushButton#settingActionButton:disabled { background: #94A3B8; }"
            "QLabel#settingActionStatus { color: %5; }"
            "QLabel#settingsShortcutValue { color: #3B82F6; font-weight: 600; }"
            "QLabel#settingsFixedValue { color: %5; }"
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
