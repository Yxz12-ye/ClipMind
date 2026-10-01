#include <QCoreApplication>
#include <QVector>
#include <gtest/gtest.h>

#include "service/SettingRegistry.hpp"

namespace {

const QString kPageId = QStringLiteral("about");
const QString kGroupId = QStringLiteral("version");
const QString kVersionGroupId = QStringLiteral("core/version");

// 注册一个只含「关于 / 版本信息」的最小 core 插件, 返回可继续注册的句柄
PluginSettings registerAboutPage(SettingRegistry& registry) {
    PluginSettings core = registry.registerPlugin(QStringLiteral("core"));
    EXPECT_TRUE(core.registerPage(kPageId, QStringLiteral("关于"), QString(), 40));
    EXPECT_TRUE(core.registerGroup(kPageId, kGroupId, QStringLiteral("版本信息")));
    return core;
}

}  // namespace

TEST(SettingRegistryTest, FixedTextSettingKeepsTextAndIsNotPersistent) {
    SettingRegistry registry;
    PluginSettings core = registerAboutPage(registry);

    const QString key = QStringLiteral("softwareVersion");
    const QString title = QStringLiteral("软件版本");
    const QString description = QStringLiteral("当前安装的 ClipMind 版本号");
    const QString version = QStringLiteral("0.1.0");
    ASSERT_TRUE(core.registerFixedText(kGroupId, key, title, description, version));

    const QVector<SettingDefinition> settings = registry.settings(kVersionGroupId);
    ASSERT_EQ(settings.size(), 1);
    EXPECT_EQ(settings.front().key, QStringLiteral("core/softwareVersion"));
    EXPECT_EQ(settings.front().pageId, QStringLiteral("core/about"));
    EXPECT_EQ(settings.front().type, SettingType::FixedText);
    EXPECT_EQ(settings.front().defaultValue.toString(), version);
    // 固定文本项只读, 不需要写进配置文件
    EXPECT_FALSE(settings.front().isPersistent());
}

TEST(SettingRegistryTest, EditableSettingsStayPersistent) {
    SettingRegistry registry;
    PluginSettings core = registerAboutPage(registry);

    const QString boolKey = QStringLiteral("showInTray");
    const QString stringKey = QStringLiteral("channel");
    const QString title = QStringLiteral("更新通道");
    const QString channel = QStringLiteral("stable");
    ASSERT_TRUE(core.registerBool(kGroupId, boolKey, title, QString(), true));
    ASSERT_TRUE(core.registerString(kGroupId, stringKey, title, QString(), channel));

    const QVector<SettingDefinition> settings = registry.settings(kVersionGroupId);
    ASSERT_EQ(settings.size(), 2);
    for (const SettingDefinition& setting : settings) {
        EXPECT_TRUE(setting.isPersistent());
    }
}

TEST(SettingRegistryTest, FixedTextSettingRejectsEmptyText) {
    SettingRegistry registry;
    PluginSettings core = registerAboutPage(registry);

    const QString key = QStringLiteral("buildTime");
    const QString title = QStringLiteral("构建时间");
    EXPECT_FALSE(core.registerFixedText(kGroupId, key, title, QString(), QString()));
    EXPECT_TRUE(registry.settings(kVersionGroupId).isEmpty());
}

TEST(SettingRegistryTest, SealedRegistryRejectsFixedText) {
    SettingRegistry registry;
    PluginSettings core = registerAboutPage(registry);
    registry.seal();

    const QString key = QStringLiteral("softwareVersion");
    const QString title = QStringLiteral("软件版本");
    const QString version = QStringLiteral("0.1.0");
    EXPECT_FALSE(core.registerFixedText(kGroupId, key, title, QString(), version));
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
