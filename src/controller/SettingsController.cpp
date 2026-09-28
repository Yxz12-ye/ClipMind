#include "SettingsController.hpp"

#include <QWidget>

#include "service/LogService.hpp"
#include "service/SQLService.hpp"
#include "service/SettingRegistry.hpp"
#include "service/SettingService.hpp"
#include "ui/SettingsDialog.hpp"

SettingsController::SettingsController(SettingRegistry* registry, SQLService* sqlService,
                                       SettingService* settingService, QObject* parent)
    : QObject(parent), registry(registry), sqlService(sqlService), settingService(settingService) {
    if (settingService != nullptr) {
        // 取值变化原样转发出去, 主窗口据此同步自己的状态
        connect(settingService, &SettingService::valueChanged, this,
                &SettingsController::settingValueChanged);
    }
}

void SettingsController::commitDefinitions() {
    if (registry == nullptr || settingService == nullptr) {
        return;
    }

    QVector<QPair<QString, QVariant>> defaultValues;
    const QVector<SettingDefinition>& definitions = registry->allSettings();
    defaultValues.reserve(definitions.size());
    for (const SettingDefinition& definition : definitions) {
        defaultValues.append({definition.key, definition.defaultValue});
    }

    if (!settingService->registerSettings(defaultValues)) {
        LogService::warn("SettingsController",
                         "commit setting definitions failed, some of {} settings are unavailable "
                         "(see warnings above)",
                         defaultValues.size());
    }
}

QVariant SettingsController::value(const QString& key) const {
    if (settingService == nullptr) {
        return {};
    }

    return settingService->get(key);
}

QHash<QString, QVariant> SettingsController::collectValues() const {
    QHash<QString, QVariant> values;
    if (registry == nullptr) {
        return values;
    }

    for (const SettingDefinition& definition : registry->allSettings()) {
        values.insert(definition.key, value(definition.key));
    }
    return values;
}

void SettingsController::openDialog(QWidget* parent) {
    if (registry == nullptr || sqlService == nullptr || settingService == nullptr) {
        LogService::warn("SettingsController",
                         "settings controller is not fully wired, settings dialog skipped");
        return;
    }

    SettingsDialog dialog(registry, parent);

    // ① 视图不读数据源: 设置项当前值与标签列表都由这里推入
    dialog.setSettingValues(collectValues());
    reloadTags(dialog);

    // ② 设置项: 用户改动 -> SettingService 落库 -> 值变化回到界面刷新显示
    connect(&dialog, &SettingsDialog::valueChanged, this,
            [this](const QString& key, const QVariant& newValue) {
                settingService->set(key, newValue);
            });
    connect(settingService, &SettingService::valueChanged, &dialog,
            &SettingsDialog::setSettingValue);

    // ③ 标签: 视图只发意图, 校验与落库都在这里, 完成后用 setTags() 推回权威列表
    connect(&dialog, &SettingsDialog::tagAddRequested, this,
            [this, &dialog](const Tag& tag) { handleTagAdd(dialog, tag); });
    connect(&dialog, &SettingsDialog::tagUpdateRequested, this,
            [this, &dialog](const QString& originalName, const Tag& tag) {
                handleTagUpdate(dialog, originalName, tag);
            });
    connect(&dialog, &SettingsDialog::tagDeleteRequested, this,
            [this, &dialog](const QString& tagName) { handleTagDelete(dialog, tagName); });
    connect(&dialog, &SettingsDialog::tagReorderRequested, this,
            [this, &dialog](const QStringList& orderedNames) {
                handleTagReorder(dialog, orderedNames);
            });

    dialog.exec();
}

void SettingsController::reloadTags(SettingsDialog& dialog) const {
    dialog.setTags(sqlService->getTags());
}

bool SettingsController::containsTagName(const QString& name) const {
    for (const Tag& tag : sqlService->getTags()) {
        if (tag.tagName == name) {
            return true;
        }
    }
    return false;
}

void SettingsController::handleTagAdd(SettingsDialog& dialog, const Tag& tag) {
    // SQLService::save() 碰到重名会静默地什么都不做, 所以重名必须在这里先拦住
    if (containsTagName(tag.tagName)) {
        dialog.showTagError(QStringLiteral("添加标签"),
                            QStringLiteral("已存在同名标签，请更换名称"));
        return;
    }

    const QString error = sqlService->save(tag);
    if (!error.isEmpty()) {
        dialog.showTagError(QStringLiteral("添加标签"), error);
        return;
    }

    reloadTags(dialog);
}

void SettingsController::handleTagUpdate(SettingsDialog& dialog, const QString& originalName,
                                         const Tag& tag) {
    if (originalName != tag.tagName && containsTagName(tag.tagName)) {
        dialog.showTagError(QStringLiteral("编辑标签"),
                            QStringLiteral("已存在同名标签，请更换名称"));
        return;
    }

    if (!sqlService->updateTag(originalName, tag)) {
        dialog.showTagError(QStringLiteral("编辑标签"), QStringLiteral("保存失败，请稍后重试"));
        return;
    }

    reloadTags(dialog);
}

void SettingsController::handleTagDelete(SettingsDialog& dialog, const QString& tagName) {
    if (!sqlService->deleteTag(tagName)) {
        dialog.showTagError(QStringLiteral("删除标签"), QStringLiteral("删除失败，请稍后重试"));
        return;
    }

    reloadTags(dialog);
}

void SettingsController::handleTagReorder(SettingsDialog& dialog, const QStringList& orderedNames) {
    // 排序失败时照样把数据库里的顺序推回去, 界面因此自动回到改动前的样子
    if (!sqlService->reorderTags(orderedNames)) {
        dialog.showTagError(QStringLiteral("排序标签"),
                            QStringLiteral("排序保存失败，已恢复原顺序"));
    }

    reloadTags(dialog);
}
