#include "SettingsController.hpp"

#include <QWidget>

#include "service/EmbeddingService.hpp"
#include "service/LogService.hpp"
#include "service/SQLService.hpp"
#include "service/SettingRegistry.hpp"
#include "service/SettingService.hpp"
#include "ui/SettingsDialog.hpp"

namespace {

// 向量化接口的键名与动作键: 注册在 MainWindow, 这里按 key 消费
const QString kEmbeddingUrlModeKey = QStringLiteral("core/embeddingUrlMode");
const QString kEmbeddingUrlKey = QStringLiteral("core/embeddingUrl");
const QString kEmbeddingModelKey = QStringLiteral("core/embeddingModel");
const QString kEmbeddingTestAction = QStringLiteral("core/embeddingTest");

// Base URL 的候选项 value(见 MainWindow 的注册), 用于还原 URL 拼接方式
const QString kEmbeddingUrlModeBase = QStringLiteral("base");

// 测试用的固定文本, 只关心接口能否返回向量
const QString kEmbeddingTestText = QStringLiteral("ClipMind 文本向量化接口测试");

}  // namespace

SettingsController::SettingsController(SettingRegistry* registry, SQLService* sqlService,
                                       SettingService* settingService,
                                       EmbeddingService* embeddingService, QObject* parent)
    : QObject(parent),
      registry(registry),
      sqlService(sqlService),
      settingService(settingService),
      embeddingService(embeddingService) {
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

    QVector<QPair<QString, QVariant>> defaultValues;  // 用户偏好, 整批只落盘一次
    QVector<QPair<QString, QVariant>> fixedValues;    // 固定文本项, 只进内存
    const QVector<SettingDefinition>& definitions = registry->allSettings();
    defaultValues.reserve(definitions.size());
    for (const SettingDefinition& definition : definitions) {
        if (!definition.storesValue()) {
            continue;  // 动作项只是一个按钮, 不占 SettingService 的键
        }

        if (definition.isPersistent()) {
            defaultValues.append({definition.key, definition.defaultValue});
        } else {
            fixedValues.append({definition.key, definition.defaultValue});
        }
    }

    // 固定文本项虽然不写配置文件, 但同样要注册: 之后 value() 与界面推值才能走同一条路
    const bool persistentCommitted = settingService->registerSettings(defaultValues);
    const bool fixedCommitted = settingService->registerSettings(fixedValues, false);
    if (!persistentCommitted || !fixedCommitted) {
        LogService::warn("SettingsController",
                         "commit setting definitions failed, some of {} settings are unavailable "
                         "(see warnings above)",
                         definitions.size());
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
        if (!definition.storesValue()) {
            continue;  // 动作项没有取值, 推给界面只会污染它的结果提示
        }

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

    // ③ 动作项: 视图只上报键名, 执行与结果提示都在这里
    connect(&dialog, &SettingsDialog::actionTriggered, this,
            [this, &dialog](const QString& key) { handleAction(dialog, key); });
    if (embeddingService != nullptr) {
        // 异步结果回到界面: 上下文对象是对话框, 对话框析构后连接自动失效
        connect(embeddingService, &EmbeddingService::embeddingSucceeded, &dialog,
                [this, &dialog](quint64 requestId, const EmbeddingResult& result) {
                    if (requestId != embeddingRequestId) {
                        return;  // 属于上一次已经作废的请求
                    }

                    embeddingRequestId = 0;
                    dialog.setActionStatus(
                        kEmbeddingTestAction,
                        QStringLiteral("成功：%1 维向量，耗时 %2 ms")
                            .arg(result.embedding.size())
                            .arg(embeddingTestTimer.elapsed()),
                        SettingActionState::Success);
                });
        connect(embeddingService, &EmbeddingService::embeddingFailed, &dialog,
                [this, &dialog](quint64 requestId, const EmbeddingError& error) {
                    if (requestId != embeddingRequestId) {
                        return;
                    }

                    embeddingRequestId = 0;
                    const QString status =
                        error.httpStatus > 0
                            ? QStringLiteral("失败（HTTP %1）：%2")
                                  .arg(error.httpStatus)
                                  .arg(error.message)
                            : QStringLiteral("失败：%1").arg(error.message);
                    dialog.setActionStatus(kEmbeddingTestAction, status,
                                           SettingActionState::Failure);
                });
    }

    // ④ 标签: 视图只发意图, 校验与落库都在这里, 完成后用 setTags() 推回权威列表
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

    // 关掉对话框时放弃还没回来的测试请求, 结果也没人接得住了
    if (embeddingService != nullptr && embeddingRequestId != 0) {
        const quint64 requestId = embeddingRequestId;
        embeddingRequestId = 0;  // 先作废: 取消会立刻触发失败回调, 不该再往界面上写提示
        embeddingService->cancelRequest(requestId);
    }
}

void SettingsController::handleAction(SettingsDialog& dialog, const QString& key) {
    if (key == kEmbeddingTestAction) {
        testEmbeddingEndpoint(dialog);
        return;
    }

    LogService::warn("SettingsController", "no handler for setting action: {}", key);
}

void SettingsController::testEmbeddingEndpoint(SettingsDialog& dialog) {
    if (embeddingService == nullptr || embeddingRequestId != 0) {
        return;  // 没有向量化服务, 或者已经有一次测试在跑
    }

    // 配置就存在 SettingService 里, 用时现取, 保证测试用的就是界面上的最新值
    EmbeddingConfig config;
    config.url = value(kEmbeddingUrlKey).toString();
    config.model = value(kEmbeddingModelKey).toString();
    config.urlMode = value(kEmbeddingUrlModeKey).toString() == kEmbeddingUrlModeBase
                         ? EmbeddingUrlMode::BaseUrl
                         : EmbeddingUrlMode::FullEndpoint;

    if (config.url.trimmed().isEmpty()) {
        dialog.setActionStatus(kEmbeddingTestAction, QStringLiteral("失败：请输入接口 URL"),
                               SettingActionState::Failure);
        return;
    }

    dialog.setActionStatus(kEmbeddingTestAction, QStringLiteral("正在请求接口…"),
                           SettingActionState::Pending);
    embeddingTestTimer.start();
    embeddingRequestId = embeddingService->embedText(kEmbeddingTestText, config);
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
