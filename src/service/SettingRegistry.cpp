#include "SettingRegistry.hpp"

#include <algorithm>
#include <utility>

#include "LogService.hpp"

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

bool PluginSettings::registerFixedText(const QString& groupId, const QString& key,
                                       const QString& title, const QString& description,
                                       const QString& value, int order) {
    return registry != nullptr &&
           registry->registerSetting(pluginId, groupId, key, title, description,
                                     SettingType::FixedText, value, {}, order);
}

SettingRegistry::SettingRegistry(QObject* parent) : QObject(parent) {}

PluginSettings SettingRegistry::registerPlugin(const QString& pluginId) {
    // 返回空句柄而非报错: registry 为空时其成员方法都会直接返回 false
    if (!validIdentifier(pluginId)) {
        LogService::warn("SettingRegistry", "invalid plugin id: {}", pluginId);
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

const QVector<SettingDefinition>& SettingRegistry::allSettings() const {
    return settingDefinitions;
}

bool SettingRegistry::registerPage(const QString& pluginId, const QString& pageId,
                                   const QString& title, const QString& description, int order) {
    const QString fullId = namespacedId(pluginId, pageId);
    if (!canRegister(pluginId, pageId) || title.trimmed().isEmpty() ||
        containsId(pageDefinitions, fullId)) {
        LogService::warn("SettingRegistry", "unable to register settings page: {}", fullId);
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
        LogService::warn("SettingRegistry", "unable to register settings group: {}", fullGroupId);
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
    // 固定文本项没有可编辑的取值, 空文本只会在设置页渲染出一个空行
    const bool validFixedText =
        type != SettingType::FixedText || !defaultValue.toString().isEmpty();
    if (!canRegister(pluginId, key) || title.trimmed().isEmpty() || !groupExists ||
        containsKey(settingDefinitions, fullKey) || !defaultValue.isValid() || !validEnum ||
        !validFixedText) {
        LogService::warn("SettingRegistry", "unable to register setting: {}", fullKey);
        return false;
    }

    // 这里只收集定义, 不碰持久化: 默认值由 Controller 在所有注册结束后统一提交
    // (见 SettingRegistry 类注释里的典型流程)

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
    // 注册阶段只看定义本身是否合法, 服务是否就绪留给提交阶段判断
    return !sealed && validIdentifier(pluginId) && validIdentifier(localId);
}
