#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVector>

class SettingService;

enum class SettingType {
    Boolean,
    String,
    Integer,
    Enum,
};

struct SettingOption {
    QString value;
    QString label;
};

struct SettingPageDefinition {
    QString id;
    QString title;
    QString description;
    int order = 0;
};

struct SettingGroupDefinition {
    QString id;
    QString pageId;
    QString title;
    int order = 0;
};

struct SettingDefinition {
    QString key;
    QString pageId;
    QString groupId;
    QString title;
    QString description;
    SettingType type = SettingType::String;
    QVariant defaultValue;
    QVector<SettingOption> options;
    int order = 0;
};

class SettingRegistry;

class PluginSettings {
public:
    PluginSettings() = default;

    bool registerPage(const QString& pageId, const QString& title, const QString& description,
                      int order = 0);
    bool registerGroup(const QString& pageId, const QString& groupId, const QString& title,
                       int order = 0);
    bool registerBool(const QString& groupId, const QString& key, const QString& title,
                      const QString& description, bool defaultValue, int order = 0);
    bool registerString(const QString& groupId, const QString& key, const QString& title,
                        const QString& description, const QString& defaultValue, int order = 0);
    bool registerInt(const QString& groupId, const QString& key, const QString& title,
                     const QString& description, int defaultValue, int order = 0);
    bool registerEnum(const QString& groupId, const QString& key, const QString& title,
                      const QString& description, const QVector<SettingOption>& options,
                      const QString& defaultValue, int order = 0);

private:
    friend class SettingRegistry;
    PluginSettings(SettingRegistry* registry, QString pluginId);

    SettingRegistry* registry = nullptr;
    QString pluginId;
};

class SettingRegistry : public QObject {
    Q_OBJECT

public:
    explicit SettingRegistry(SettingService* service, QObject* parent = nullptr);

    PluginSettings registerPlugin(const QString& pluginId);
    void seal();
    bool isSealed() const;

    const QVector<SettingPageDefinition>& pages() const;
    QVector<SettingGroupDefinition> groups(const QString& pageId) const;
    QVector<SettingDefinition> settings(const QString& groupId) const;

    SettingService* service() const;

private:
    friend class PluginSettings;

    bool registerPage(const QString& pluginId, const QString& pageId, const QString& title,
                      const QString& description, int order);
    bool registerGroup(const QString& pluginId, const QString& pageId, const QString& groupId,
                       const QString& title, int order);
    bool registerSetting(const QString& pluginId, const QString& groupId, const QString& key,
                         const QString& title, const QString& description, SettingType type,
                         const QVariant& defaultValue, const QVector<SettingOption>& options,
                         int order);

    QString namespacedId(const QString& pluginId, const QString& localId) const;
    bool canRegister(const QString& pluginId, const QString& localId) const;

    SettingService* settingService;
    QVector<SettingPageDefinition> pageDefinitions;
    QVector<SettingGroupDefinition> groupDefinitions;
    QVector<SettingDefinition> settingDefinitions;
    bool sealed = false;
};
