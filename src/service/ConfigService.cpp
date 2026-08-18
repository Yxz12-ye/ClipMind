#include "ConfigService.hpp"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

constexpr auto kConfigFileName = "config.json";

QString urlModeName(EmbeddingUrlMode mode) {
    return mode == EmbeddingUrlMode::BaseUrl ? QStringLiteral("base") : QStringLiteral("full");
}

EmbeddingUrlMode urlModeFromName(const QString& name) {
    return name == QLatin1String("base") ? EmbeddingUrlMode::BaseUrl
                                         : EmbeddingUrlMode::FullEndpoint;
}

}  // namespace

ConfigService::ConfigService(const QDir& configDirectory) : configDirectory(configDirectory) {}

ApplicationSettings ConfigService::load() const {
    ApplicationSettings settings;
    const QString filePath = configFilePath();
    QFile file(filePath);
    if (!file.exists()) {
        return settings;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "open config file failed:" << filePath << file.errorString();
        return settings;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "parse config file failed:" << filePath << error.errorString();
        return settings;
    }

    const QJsonObject root = document.object();
    settings.hideAfterPaste = root.value(QStringLiteral("hideAfterPaste")).toBool(true);
    settings.showTrayIcon = root.value(QStringLiteral("showTrayIcon")).toBool(true);

    const QJsonObject embedding = root.value(QStringLiteral("embedding")).toObject();
    settings.embeddingConfig.url = embedding.value(QStringLiteral("url")).toString();
    settings.embeddingConfig.model = embedding.value(QStringLiteral("model")).toString();
    settings.embeddingConfig.urlMode =
        urlModeFromName(embedding.value(QStringLiteral("urlMode")).toString());
    return settings;
}

bool ConfigService::save(const ApplicationSettings& settings) const {
    if (!configDirectory.exists() && !QDir().mkpath(configDirectory.absolutePath())) {
        qWarning() << "create config directory failed:" << configDirectory.absolutePath();
        return false;
    }

    QJsonObject embedding;
    embedding.insert(QStringLiteral("url"), settings.embeddingConfig.url);
    embedding.insert(QStringLiteral("model"), settings.embeddingConfig.model);
    embedding.insert(QStringLiteral("urlMode"), urlModeName(settings.embeddingConfig.urlMode));

    QJsonObject root;
    root.insert(QStringLiteral("hideAfterPaste"), settings.hideAfterPaste);
    root.insert(QStringLiteral("showTrayIcon"), settings.showTrayIcon);
    root.insert(QStringLiteral("embedding"), embedding);

    const QString filePath = configFilePath();
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "open config file for writing failed:" << filePath << file.errorString();
        return false;
    }

    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) == -1 || !file.commit()) {
        qWarning() << "save config file failed:" << filePath << file.errorString();
        return false;
    }

    return true;
}

QString ConfigService::configFilePath() const {
    return configDirectory.filePath(QString::fromLatin1(kConfigFileName));
}
