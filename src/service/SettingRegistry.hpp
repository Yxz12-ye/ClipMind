#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVector>

class SettingService;

// 设置项的取值类型, 决定设置页用哪种编辑控件, 也决定默认值如何被 SettingService 还原
enum class SettingType {
    Boolean,  // 复选框
    String,   // 单行文本框
    Integer,  // 整数输入框
    Enum,     // 下拉框, 取值必须是 options 中的某一项
};

// 枚举型设置的一个候选项: value 用于持久化, label 用于界面显示
struct SettingOption {
    QString value;
    QString label;
};

// 设置页分类, 对应设置对话框左侧列表中的一项
struct SettingPageDefinition {
    QString id;           // 带插件命名空间的完整 id, 形如 `core/general`
    QString title;        // 左侧列表显示的名称
    QString description;  // 页面说明, 目前仅在注册时记录
    int order = 0;        // 升序排列, 数值小的页面靠前
};

// 设置分组, 对应设置页内的一组设置项
struct SettingGroupDefinition {
    QString id;      // 带插件命名空间的完整 id, 形如 `core/behavior`
    QString pageId;  // 所属页面的完整 id
    QString title;   // 分组标题
    int order = 0;   // 同一页面内的升序排列
};

// 单个设置项的完整元数据
struct SettingDefinition {
    QString key;          // 带插件命名空间的完整键名, 形如 `core/hideAfterPaste`
    QString pageId;       // 所属页面的完整 id(注册分组时自动带出, 便于按页检索)
    QString groupId;      // 所属分组的完整 id
    QString title;        // 设置项标题
    QString description;  // 设置项说明, 显示在标题下方
    SettingType type = SettingType::String;
    QVariant defaultValue;           // 注册时的默认值, 同时记录了类型
    QVector<SettingOption> options;  // 仅 Enum 类型使用
    int order = 0;                   // 同一分组内的升序排列
};

class SettingRegistry;

/**
 * @brief 插件侧的设置注册句柄, 由 SettingRegistry::registerPlugin() 创建
 *
 * 该对象把 `pluginId` 与调用方绑定, 因此各 register* 方法不必重复传入插件名;
 * 其内部只转发到 SettingRegistry, 不持有任何定义数据, 可以按值自由拷贝。
 *
 * 所有方法都返回注册是否被接受: 传入的 id 非法、标题为空、重名、
 * registry 已 seal 或 SettingService 未就绪时返回 `false` 并打印警告,
 * 调用方可据此判断插件设置是否真正生效。
 */
class PluginSettings {
public:
    PluginSettings() = default;

    /**
     * @brief 注册一个设置页
     * @param pageId 页面的局部 id, 会与插件名拼接成 `pluginId/pageId`
     * @param title 页面标题, 不能为空
     * @param description 页面说明
     * @param order 页面排序值, 越小越靠前
     * @return 注册成功返回 `true`
     */
    bool registerPage(const QString& pageId, const QString& title, const QString& description,
                      int order = 0);
    /**
     * @brief 在已注册的页面上注册一个分组
     * @param pageId 已注册页面的局部 id
     * @param groupId 分组的局部 id
     * @param title 分组标题, 不能为空
     * @param order 分组排序值, 越小越靠前
     * @return 注册成功返回 `true`; 页面不存在时返回 `false`
     */
    bool registerGroup(const QString& pageId, const QString& groupId, const QString& title,
                       int order = 0);
    /**
     * @brief 注册一个布尔型设置项
     * @param groupId 已注册分组的局部 id
     * @param key 设置的局部键名, 最终键为 `pluginId/key`
     * @param title 设置项标题, 不能为空
     * @param description 设置项说明
     * @param defaultValue 默认值, 同时作为 SettingService 中该键的初始值
     * @param order 同一分组内的排序值, 越小越靠前
     * @return 注册成功返回 `true`
     */
    bool registerBool(const QString& groupId, const QString& key, const QString& title,
                      const QString& description, bool defaultValue, int order = 0);
    /**
     * @brief 注册一个字符串型设置项, 参数含义同 registerBool()
     */
    bool registerString(const QString& groupId, const QString& key, const QString& title,
                        const QString& description, const QString& defaultValue, int order = 0);
    /**
     * @brief 注册一个整型设置项, 参数含义同 registerBool()
     */
    bool registerInt(const QString& groupId, const QString& key, const QString& title,
                     const QString& description, int defaultValue, int order = 0);
    /**
     * @brief 注册一个枚举型设置项
     * @param options 候选项列表, `defaultValue` 必须能在其中找到对应的 value
     * @param defaultValue 默认候选项的 value
     * @return 注册成功返回 `true`; 默认值不在候选项中时返回 `false`
     *
     * 其余参数含义同 registerBool()。
     */
    bool registerEnum(const QString& groupId, const QString& key, const QString& title,
                      const QString& description, const QVector<SettingOption>& options,
                      const QString& defaultValue, int order = 0);

private:
    friend class SettingRegistry;
    // 只允许 SettingRegistry::registerPlugin() 构造, 保证 pluginId 一定经过校验
    PluginSettings(SettingRegistry* registry, QString pluginId);

    SettingRegistry* registry = nullptr;  // 为空时所有注册调用都安全地返回 false
    QString pluginId;                     // 已去除首尾空白的插件 id
};

/**
 * @brief 插件设置的元数据中心, 统一收拢页面/分组/设置项的定义并转发持久化
 *
 * 各插件通过 registerPlugin() 取得句柄后注册自己的界面结构, 设置对话框只读取本类
 * 提供的定义来动态生成界面; 真正负责读写 config.json 的是 SettingService, 本类在
 * 注册设置项时把默认值转交给它, 并复用其信号做界面同步。
 *
 * 典型流程(见 MainWindow 构造):
 * @code
 * auto core = registry->registerPlugin(QStringLiteral("core"));
 * core.registerPage(QStringLiteral("general"), ...);
 * core.registerGroup(QStringLiteral("general"), QStringLiteral("behavior"), ...);
 * core.registerBool(QStringLiteral("behavior"), QStringLiteral("hideAfterPaste"), ...);
 * registry->seal();  // 所有插件注册完毕后封盘
 * @endcode
 *
 * 线程约束: 与 SettingService 一样只在 GUI 线程使用。
 */
class SettingRegistry : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造注册中心
     * @param service 提供持久化能力的服务, 可为空(此时所有注册都会失败)
     * @param parent Qt 父对象
     */
    explicit SettingRegistry(SettingService* service, QObject* parent = nullptr);

    /**
     * @brief 取得某个插件的注册句柄
     * @param pluginId 插件标识, 不能为空且不能包含 `/`
     * @return 绑定该插件的句柄; pluginId 非法时返回一个不关联 registry 的空句柄,
     *         后续调用会返回 `false` 而不会崩溃
     */
    PluginSettings registerPlugin(const QString& pluginId);
    /**
     * @brief 封盘, 封盘后不再接受任何注册
     *
     * 应在所有插件注册完设置后调用, 以保证启动完成后定义集合不再变化。
     */
    void seal();
    /**
     * @brief 查询是否已封盘
     */
    bool isSealed() const;

    /**
     * @brief 获取全部设置页, 已按 `order` 升序排列
     * @return 内部容器的常量引用, 注意其生命周期与本对象绑定
     */
    const QVector<SettingPageDefinition>& pages() const;
    /**
     * @brief 获取指定页面下的分组
     * @param pageId 页面的完整 id, 形如 `core/general`
     * @return 该页面下的分组副本, 已按 `order` 升序排列; 页面不存在时返回空列表
     */
    QVector<SettingGroupDefinition> groups(const QString& pageId) const;
    /**
     * @brief 获取指定分组下的设置项
     * @param groupId 分组的完整 id, 形如 `core/behavior`
     * @return 该分组下的设置项副本, 已按 `order` 升序排列; 分组不存在时返回空列表
     */
    QVector<SettingDefinition> settings(const QString& groupId) const;

    /**
     * @brief 获取底层设置服务, 供设置界面直接读写具体取值
     * @return 构造时传入的服务指针, 可能为空
     */
    SettingService* service() const;

private:
    friend class PluginSettings;

    // 以下三个方法是 PluginSettings 的实际实现, 除 pluginId 外还会做命名空间拼接与校验
    bool registerPage(const QString& pluginId, const QString& pageId, const QString& title,
                      const QString& description, int order);
    bool registerGroup(const QString& pluginId, const QString& pageId, const QString& groupId,
                       const QString& title, int order);
    bool registerSetting(const QString& pluginId, const QString& groupId, const QString& key,
                         const QString& title, const QString& description, SettingType type,
                         const QVariant& defaultValue, const QVector<SettingOption>& options,
                         int order);

    // 把插件局部 id 拼接为全局唯一的 `pluginId/localId`
    QString namespacedId(const QString& pluginId, const QString& localId) const;
    // 注册前置条件: 未封盘、id 合法、设置服务已就绪
    bool canRegister(const QString& pluginId, const QString& localId) const;

    SettingService* settingService;
    QVector<SettingPageDefinition> pageDefinitions;    // 按 order 升序维护, 有新增即重排
    QVector<SettingGroupDefinition> groupDefinitions;  // 按注册顺序存放, 查询时再排序
    QVector<SettingDefinition> settingDefinitions;     // 同上
    bool sealed = false;                               // 封盘后 canRegister() 一律失败
};
