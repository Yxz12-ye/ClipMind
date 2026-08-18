#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "service/ConfigService.hpp"

TEST(ConfigServiceTest, SavesSettingsAsJsonAndRestoresThem) {
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QDir configDirectory(temporaryDirectory.filePath(QStringLiteral("data")));
    ConfigService service(configDirectory);
    ApplicationSettings expected;
    expected.hideAfterPaste = false;
    expected.showTrayIcon = false;
    expected.embeddingConfig.url = QStringLiteral("http://localhost:11434/v1");
    expected.embeddingConfig.model = QStringLiteral("nomic-embed-text");
    expected.embeddingConfig.urlMode = EmbeddingUrlMode::BaseUrl;

    ASSERT_TRUE(service.save(expected));

    QFile file(configDirectory.filePath(QStringLiteral("config.json")));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    ASSERT_TRUE(document.isObject());
    EXPECT_FALSE(document.object().value(QStringLiteral("hideAfterPaste")).toBool());
    EXPECT_FALSE(document.object().value(QStringLiteral("showTrayIcon")).toBool());

    const ApplicationSettings actual = service.load();
    EXPECT_EQ(actual.hideAfterPaste, expected.hideAfterPaste);
    EXPECT_EQ(actual.showTrayIcon, expected.showTrayIcon);
    EXPECT_EQ(actual.embeddingConfig.url, expected.embeddingConfig.url);
    EXPECT_EQ(actual.embeddingConfig.model, expected.embeddingConfig.model);
    EXPECT_EQ(actual.embeddingConfig.urlMode, expected.embeddingConfig.urlMode);
}

TEST(ConfigServiceTest, MissingOrInvalidConfigUsesDefaultSettings) {
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QDir configDirectory(temporaryDirectory.path());
    ConfigService service(configDirectory);

    ApplicationSettings settings = service.load();
    EXPECT_TRUE(settings.hideAfterPaste);
    EXPECT_TRUE(settings.showTrayIcon);
    EXPECT_TRUE(settings.embeddingConfig.url.isEmpty());
    EXPECT_TRUE(settings.embeddingConfig.model.isEmpty());
    EXPECT_EQ(settings.embeddingConfig.urlMode, EmbeddingUrlMode::FullEndpoint);

    QFile file(configDirectory.filePath(QStringLiteral("config.json")));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write("invalid json"), 12);
    file.close();

    settings = service.load();
    EXPECT_TRUE(settings.hideAfterPaste);
    EXPECT_TRUE(settings.showTrayIcon);
    EXPECT_TRUE(settings.embeddingConfig.url.isEmpty());
    EXPECT_TRUE(settings.embeddingConfig.model.isEmpty());
    EXPECT_EQ(settings.embeddingConfig.urlMode, EmbeddingUrlMode::FullEndpoint);
}
