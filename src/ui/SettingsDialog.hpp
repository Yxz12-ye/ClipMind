#pragma once

#include <QDialog>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include "struct.hpp"  // 信号/槽按值传递 Tag, moc 生成元对象时需要完整定义

class QEvent;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QVBoxLayout;
class SettingEditor;
class SettingRegistry;

class CustomHead;
struct SettingDefinition;
struct SettingPageDefinition;

// 针对重复代码重新制作的Widget工厂, 所有可复用组件都应由工厂创建
namespace WidgetFactory {
// 页面骨架: 标题 + 说明 + 内容布局(pageId 会写进 objectName, 便于给单个页面单独写样式)
QWidget* createPage(const QString& pageId, QWidget* parent, const QString& title,
                    const QString& brief);
// 分组骨架: 组标题 + 内容布局
QWidget* createGroup(const QString& groupId, QWidget* parent, const QString& title);
// 设置行: 左侧标题/说明, 右侧控件
QWidget* createRow(const QString& title, const QString& description, QWidget* control,
                   QWidget* parent);
// 取出 createPage()/createGroup() 建好的内容布局, 供调用方继续追加子项; 取不到时返回 nullptr
QVBoxLayout* contentLayout(QWidget* container);
}  // namespace WidgetFactory

/**
 * @brief 设置对话框, 只承担 View 职责
 *
 * 职责边界:
 * - 结构: 页面/分组/设置项全部按 SettingRegistry 的定义动态生成, 除标签页外不做 pageId 分支;
 * - 显示值: 设置项初值只读地取自 SettingService, 之后由外部调用 setSettingValue() 刷新显示;
 *   标签列表完全由外部通过 setTags() 推送, 本类不再持有 SQLService;
 * - 用户操作: 一律转成信号交给 Controller, 本类不写 SettingService, 也不增删改标签数据。
 *
 * Controller 侧的典型接法:
 * @code
 * connect(dialog, &SettingsDialog::valueChanged, service, &SettingService::set);
 * connect(dialog, &SettingsDialog::tagAddRequested, controller, &SettingsController::addTag);
 * dialog.setTags(controller->getTags());
 * @endcode
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(SettingRegistry* registry, QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

public slots:
    // 刷新单个设置项的显示值(只动界面, 不会回发 valueChanged 信号)
    void setSettingValue(const QString& settingId, const QVariant& value);
    // 用外部给的完整标签列表重建标签页, 这是标签数据进入界面的唯一入口
    void setTags(const QVector<Tag>& tags);
    // 标签操作失败时由 Controller 调用, 提示文案由调用方决定
    void showTagError(const QString& title, const QString& message);

signals:
    // 对应设置项发出更改请求(设置项id(完整键值, 可以参考QHash<QString, SettingEditor*> editors;)
    // 更改后的值)
    void valueChanged(const QString& settingId, const QVariant& value);
    // 标签操作请求: 本类只收集用户意图, 数据变更由 Controller 完成后调用 setTags() 推回界面
    void tagAddRequested(const Tag& tag);
    void tagUpdateRequested(const QString& originalName, const Tag& tag);
    void tagDeleteRequested(const QString& tagName);
    void tagReorderRequested(const QStringList& orderedNames);

protected:
    void changeEvent(QEvent* event) override;

private:
    SettingRegistry* registry = nullptr;
    CustomHead* head = nullptr;
    QListWidget* categories = nullptr;
    QStackedWidget* pages = nullptr;
    QListWidget* tagList = nullptr;
    // 设置项完整键名->Editor(持有对应的Widget)
    QHash<QString, SettingEditor*> editors;

    void setupUI();
    void applyTheme();
    // 按注册表定义生成一个通用设置页
    QWidget* createStandardPage(const SettingPageDefinition& definition);
    // 标签页自绘, 只沿用注册表里的标题/说明文案
    QWidget* createTagPage(const QString& title, const QString& brief);
    // 把「左侧Tab项」与「堆叠页面」成对挂上, 保证两边下标一致
    void addCategory(const QString& title, QWidget* page);
    SettingEditor* createEditor(const struct SettingDefinition& setting, QWidget* parent);
    void bindEditor(const struct SettingDefinition& setting, SettingEditor* editor);

    // 标签列表的界面更新
    void addTagItem(const Tag& tag);
    void updateTagItem(QListWidgetItem* item, const Tag& tag);
    Tag tagFromItem(QListWidgetItem* item) const;
    // 用户操作 -> 信号; 界面顺序也等 setTags() 回推, 避免本地状态与数据源不一致
    void requestTagAdd();
    void requestTagEdit(QListWidgetItem* item);
    void requestTagDelete(QListWidgetItem* item);
    void requestTagMove(QListWidgetItem* item, int offset);
};
