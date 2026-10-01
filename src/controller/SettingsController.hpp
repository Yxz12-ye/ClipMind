#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "struct.hpp"

class EmbeddingService;
class SettingsDialog;
class SettingRegistry;
class SettingService;
class SQLService;

/**
 * @brief 设置链路上的协调者: 提交注册信息、串接设置对话框、落地标签操作与动作项
 *
 * 它同时充当这条链路的组合根:
 * - 启动时 commitDefinitions() 把 SettingRegistry 收集到的定义提交给 SettingService,
 *   默认值到这一步才写进 config.json, 之后 get() 才读得到值; 其中只读的固定文本项
 *   (isPersistent() 为 false)不进配置文件, 只以只读方式登记在内存里, 动作项则连键都不占;
 * - openDialog() 负责创建设置对话框, 把当前值与标签列表推给界面, 再把界面的用户
 *   意图接回来: 设置项交给 SettingService 落库, 标签交给 SQLService 校验并增删改,
 *   动作项(例如向量化接口的"测试接口"按钮)在这里执行并把结果推回界面。
 *
 * 界面(View)不持有任何服务, 服务也不认识界面, 两边都只通过本类交换数据。
 */
class SettingsController : public QObject {
    Q_OBJECT

public:
    SettingsController(SettingRegistry* registry, SQLService* sqlService,
                       SettingService* settingService, EmbeddingService* embeddingService,
                       QObject* parent = nullptr);
    ~SettingsController() override = default;

    // 提交全部已注册的设置项(默认值), 必须在任何取值/建界面之前调用一次
    void commitDefinitions();
    // 打开设置对话框: 内部完成创建、推值与信号串接, 然后 exec
    void openDialog(QWidget* parent);
    // 读取已提交设置项的当前值; 键未注册时返回无效 QVariant
    QVariant value(const QString& key) const;

signals:
    // 设置项取值发生变化(转发自 SettingService), 供主窗口等关注方同步自身状态
    void settingValueChanged(const QString& key, const QVariant& value);

private:
    // 视图不读数据源, 这些方法负责把数据推给界面
    QHash<QString, QVariant> collectValues() const;
    void reloadTags(SettingsDialog& dialog) const;
    // 标签操作: 先校验再落库, 失败用 showTagError() 提示, 最后统一把权威列表推回界面
    void handleTagAdd(SettingsDialog& dialog, const Tag& tag);
    void handleTagUpdate(SettingsDialog& dialog, const QString& originalName, const Tag& tag);
    void handleTagDelete(SettingsDialog& dialog, const QString& tagName);
    void handleTagReorder(SettingsDialog& dialog, const QStringList& orderedNames);
    bool containsTagName(const QString& name) const;
    // 动作项: 按 key 分发到具体实现(目前只有向量化接口的连通性测试)
    void handleAction(SettingsDialog& dialog, const QString& key);
    void testEmbeddingEndpoint(SettingsDialog& dialog);

    SettingRegistry* registry = nullptr;
    SQLService* sqlService = nullptr;
    SettingService* settingService = nullptr;
    EmbeddingService* embeddingService = nullptr;
    quint64 embeddingRequestId = 0;  // 非 0 表示有一次测试请求正在进行
    QElapsedTimer embeddingTestTimer;
};
