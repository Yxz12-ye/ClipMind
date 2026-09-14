#include "SettingRegistry.hpp"

#include <QDebug>
#include <algorithm>
#include <utility>

#include "SettingService.hpp"

namespace {

bool validIdentifier(const QString& value) {
    return !value.trimmed().isEmpty() && !value.contains('/');
}

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
    const bool groupExists =
        std::any_of(groupDefinitions.cbegin(), groupDefinitions.cend(),
                    [&fullGroupId](const auto& group) { return group.id == fullGroupId; });
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

    if (settingService == nullptr || !settingService->registerSetting(fullKey, defaultValue)) {
        qWarning() << "unable to register setting in service:" << fullKey;
        return false;
    }

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
    return !sealed && validIdentifier(pluginId) && validIdentifier(localId) &&
           settingService != nullptr && settingService->ready();
}
