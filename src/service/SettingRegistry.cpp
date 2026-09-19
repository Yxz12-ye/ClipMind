#include "SettingRegistry.hpp"

#include <QDebug>
#include <algorithm>
#include <utility>

#include "SettingService.hpp"

namespace {

// 局部 id 不能为空, 且不允许含 `/`: 斜杠是命名空间分隔符, 出现即无法与插件名区分
bool validIdentifier(const QString& value) {
    return !value.trimmed().isEmpty() && !value.contains('/');
}

// 按 id 判断页面/分组是否已注册, 避免重复定义
template <typename T>
bool containsId(const QVector<T>& values, const QString& id) {
    return std::any_of(values.cbegin(), values.cend(),
                       [&id](const T& value) { return value.id == id; });
}

bool containsKey(const QVector<SettingDefinition>& values, const QString& key) {
    return std::any_of(values.cbegin(), values.cend(),
                       [&key](const auto& value) { return value.key == key; });
}

}  // namespace

PluginSettings::PluginSettings(SettingRegistry* registry, QString pluginId)
    : registry(registry), pluginId(std::move(pluginId)) {}

bool PluginSettings::registerPage(const QString& pageId, const QString& title,
                                  const QString& description, int order) {
    return registry != nullptr &&
           registry->registerPage(pluginId, pageId, title, description, order);
}

bool PluginSettings::registerGroup(const QString& pageId, const QString& groupId,
                                   const QString& title, int order) {
    return registry != nullptr && registry->registerGroup(pluginId, pageId, groupId, title, order);
}

bool PluginSettings::registerBool(const QString& groupId, const QString& key, const QString& title,
                                  const QString& description, bool defaultValue, int order) {
    return registry != nullptr &&
           registry->registerSetting(pluginId, groupId, key, title, description,
                                     SettingType::Boolean, defaultValue, {}, order);
}

bool PluginSettings::registerString(const QString& groupId, const QString& key,
                                    const QString& title, const QString& description,
                                    const QString& defaultValue, int order) {
    return registry != nullptr &&
           registry->registerSetting(pluginId, groupId, key, title, description,
                                     SettingType::String, defaultValue, {}, order);
}

bool PluginSettings::registerInt(const QString& groupId, const QString& key, const QString& title,
                                 const QString& description, int defaultValue, int order) {
    return registry != nullptr &&
           registry->registerSetting(pluginId, groupId, key, title, description,
                                     SettingType::Integer, defaultValue, {}, order);
}

bool PluginSettings::registerEnum(const QString& groupId, const QString& key, const QString& title,
                                  const QString& description, const QVector<SettingOption>& options,
                                  const QString& defaultValue, int order) {
    return registry != nullptr &&
           registry->registerSetting(pluginId, groupId, key, title, description, SettingType::Enum,
                                     defaultValue, options, order);
}

SettingRegistry::SettingRegistry(SettingService* service, QObject* parent)
    : QObject(parent), settingService(service) {}

PluginSettings SettingRegistry::registerPlugin(const QString& pluginId) {
    // 返回空句柄而非报错: registry 为空时其成员方法都会直接返回 false
    if (!validIdentifier(pluginId)) {
        qWarning() << "invalid plugin id:" << pluginId;
        return {};
    }

    return PluginSettings(this, pluginId.trimmed());
}

void SettingRegistry::seal() {
    sealed = true;
}

bool SettingRegistry::isSealed() const {
    return sealed;
}

const QVector<SettingPageDefinition>& SettingRegistry::pages() const {
    return pageDefinitions;
}

QVector<SettingGroupDefinition> SettingRegistry::groups(const QString& pageId) const {
    // 返回副本: 分组按注册顺序存放, 排序只影响本次查询结果, 不改变内部存储
    QVector<SettingGroupDefinition> result;
    for (const auto& group : groupDefinitions) {
        if (group.pageId == pageId) {
            result.append(group);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto& left, const auto& right) { return left.order < right.order; });
    return result;
}

QVector<SettingDefinition> SettingRegistry::settings(const QString& groupId) const {
    QVector<SettingDefinition> result;
    for (const auto& setting : settingDefinitions) {
        if (setting.groupId == groupId) {
            result.append(setting);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto& left, const auto& right) { return left.order < right.order; });
    return result;
}

SettingService* SettingRegistry::service() const {
    return settingService;
}

bool SettingRegistry::registerPage(const QString& pluginId, const QString& pageId,
                                   const QString& title, const QString& description, int order) {
    const QString fullId = namespacedId(pluginId, pageId);
    if (!canRegister(pluginId, pageId) || title.trimmed().isEmpty() ||
        containsId(pageDefinitions, fullId)) {
        qWarning() << "unable to register settings page:" << fullId;
        return false;
    }

    pageDefinitions.append({fullId, title, description, order});
    // 页面直接对外暴露内部容器, 因此在插入处就重排, 保证 pages() 始终有序
    std::sort(pageDefinitions.begin(), pageDefinitions.end(),
              [](const auto& left, const auto& right) { return left.order < right.order; });
    return true;
}

bool SettingRegistry::registerGroup(const QString& pluginId, const QString& pageId,
                                    const QString& groupId, const QString& title, int order) {
    const QString fullPageId = namespacedId(pluginId, pageId);
    const QString fullGroupId = namespacedId(pluginId, groupId);
    const bool pageExists =
        std::any_of(pageDefinitions.cbegin(), pageDefinitions.cend(),
                    [&fullPageId](const auto& page) { return page.id == fullPageId; });
    if (!canRegister(pluginId, groupId) || title.trimmed().isEmpty() || !pageExists ||
        containsId(groupDefinitions, fullGroupId)) {
        qWarning() << "unable to register settings group:" << fullGroupId;
        return false;
    }

    groupDefinitions.append({fullGroupId, fullPageId, title, order});
    return true;
}

bool SettingRegistry::registerSetting(const QString& pluginId, const QString& groupId,
                                      const QString& key, const QString& title,
                                      const QString& description, SettingType type,
                                      const QVariant& defaultValue,
                                      const QVector<SettingOption>& options, int order) {
    const QString fullGroupId = namespacedId(pluginId, groupId);
    const QString fullKey = namespacedId(pluginId, key);
    // 分组必须是已注册的, 同时也能顺带取到所属页面
    const bool groupExists =
        std::any_of(groupDefinitions.cbegin(), groupDefinitions.cend(),
                    [&fullGroupId](const auto& group) { return group.id == fullGroupId; });
    // 枚举项的默认值必须命中某个候选项, 否则界面控件无法呈现当前取值
    const bool validEnum =
        type != SettingType::Enum ||
        std::any_of(options.cbegin(), options.cend(), [&defaultValue](const auto& option) {
            return option.value == defaultValue.toString();
        });
    if (!canRegister(pluginId, key) || title.trimmed().isEmpty() || !groupExists ||
        containsKey(settingDefinitions, fullKey) || !defaultValue.isValid() || !validEnum) {
        qWarning() << "unable to register setting:" << fullKey;
        return false;
    }

    // 先落到配置文件, 失败则整个注册作废, 避免出现"界面有项但读不到值"的状态
    if (settingService == nullptr || !settingService->registerSetting(fullKey, defaultValue)) {
        qWarning() << "unable to register setting in service:" << fullKey;
        return false;
    }

    // 取所属页面 id 一并存入定义, 方便设置界面按页检索(groupExists 已保证迭代器有效)
    const auto group =
        std::find_if(groupDefinitions.cbegin(), groupDefinitions.cend(),
                     [&fullGroupId](const auto& value) { return value.id == fullGroupId; });
    settingDefinitions.append({fullKey, group->pageId, fullGroupId, title, description, type,
                               defaultValue, options, order});
    return true;
}

QString SettingRegistry::namespacedId(const QString& pluginId, const QString& localId) const {
    return pluginId.trimmed() + QStringLiteral("/") + localId.trimmed();
}

bool SettingRegistry::canRegister(const QString& pluginId, const QString& localId) const {
    // 服务未就绪时注册会被静默丢弃, 因此必须在注册阶段就拦住
    return !sealed && validIdentifier(pluginId) && validIdentifier(localId) &&
           settingService != nullptr && settingService->ready();
}
