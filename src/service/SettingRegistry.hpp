#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVector>

// 设置项的取值类型, 决定设置页用哪种编辑控件, 也决定默认值如何被 SettingService 还原
enum class SettingType {
    Boolean,    // 复选框
    String,     // 单行文本框
    Integer,    // 整数输入框
    Enum,       // 下拉框, 取值必须是 options 中的某一项
    FixedText,  // 只读文本, 例如软件版本: 界面只展示注册时给定的文本, 用户改不了
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
    QVariant defaultValue;           // 注册时的默认值, 同时记录了类型; FixedText 存放展示文本
    QVector<SettingOption> options;  // 仅 Enum 类型使用
    int order = 0;                   // 同一分组内的升序排列

    // 该项是否需要写入配置文件: 固定文本项只存在于内存与界面上, 不是用户偏好
    bool isPersistent() const {
        return type != SettingType::FixedText;
    }
};

class SettingRegistry;

/**
 * @brief 插件侧的设置注册句柄, 由 SettingRegistry::registerPlugin() 创建
 *
 * 该对象把 `pluginId` 与调用方绑定, 因此各 register* 方法不必重复传入插件名;
 * 其内部只转发到 SettingRegistry, 不持有任何定义数据, 可以按值自由拷贝。
 *
 * 所有方法都返回注册是否被接受: 传入的 id 非法、标题为空、父级不存在、重名或
 * registry 已 seal 时返回 `false` 并打印警告。
 *
 * 注意返回 `true` 只代表"定义被接受", 不代表值已经可读: 默认值要等所有注册流程
 * 结束后由 Controller 统一提交给 SettingService, 插件设置才算真正生效。
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
     * @param defaultValue 默认值, 提交给 SettingService 时作为该键的初始值
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
    /**
     * @brief 注册一个固定文本设置项: 只读, 且不写入配置文件
     * @param value 界面上原样展示的文本, 例如软件版本号, 不能为空
     * @return 注册成功返回 `true`; `value` 为空时返回 `false`
     *
     * 典型用途是版本号、构建时间这类"只展示给用户看"的信息: 设置页把它渲染成一行只读
     * 文本, 用户改不了, 也不会占用 config.json 里的键。提交阶段它仍然会进 SettingService
     * (以只读方式), 这样 SettingController::value() 与界面刷新可以和其他设置项走同一条路。
     *
     * 其余参数含义同 registerBool()。
     */
    bool registerFixedText(const QString& groupId, const QString& key, const QString& title,
                           const QString& description, const QString& value, int order = 0);

private:
    friend class SettingRegistry;
    // 只允许 SettingRegistry::registerPlugin() 构造, 保证 pluginId 一定经过校验
    PluginSettings(SettingRegistry* registry, QString pluginId);

    SettingRegistry* registry = nullptr;  // 为空时所有注册调用都安全地返回 false
    QString pluginId;                     // 已去除首尾空白的插件 id
};

/**
 * @brief 插件设置的元数据中心, 只负责收集定义并做结构校验
 *
 * 各插件通过 registerPlugin() 取得句柄后注册自己的界面结构, 设置对话框只读取本类
 * 提供的定义来动态生成界面。本类不接触持久化: 它不持有 SettingService, 注册过程
 * 也没有任何副作用, 因此可以脱离配置文件单独构造和测试。
 *
 * 注册完成后定义集合是冻结的(seal), 之后由 Controller 用 allSettings() 取到全部
 * 设置项, 逐条调用 SettingService::registerSetting() 把默认值提交上去——"提交"是
 * 显式的一步, 顺序必须在任何 get()/建界面之前, 否则会读到未注册的空值。
 * isPersistent() 为 false 的项(固定文本)提交时走只读形式, 值只留在内存里。
 *
 * 典型流程(见 MainWindow 构造 / SettingsController):
 * @code
 * auto core = registry->registerPlugin(QStringLiteral("core"));
 * core.registerPage(QStringLiteral("general"), ...);
 * core.registerGroup(QStringLiteral("general"), QStringLiteral("behavior"), ...);
 * core.registerBool(QStringLiteral("behavior"), QStringLiteral("hideAfterPaste"), ...);
 * registry->seal();  // ① 所有插件注册完毕后封盘
 *
 * for (const SettingDefinition& def : registry->allSettings()) {  // ② 手动提交
 *     service->registerSetting(def.key, def.defaultValue, def.isPersistent());
 * }
 * @endcode
 *
 * 线程约束: 与 SettingService 一样只在 GUI 线程使用。
 */
class SettingRegistry : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造注册中心
     * @param parent Qt 父对象
     */
    explicit SettingRegistry(QObject* parent = nullptr);

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
     * @brief 获取全部设置项, 不分页面与分组
     * @return 内部容器的常量引用, 注意其生命周期与本对象绑定
     *
     * 提交默认值、整体自检这类需要遍历所有设置项的场景走这里, 免得按
     * pages() -> groups() -> settings() 递归而漏掉某一层。
     */
    const QVector<SettingDefinition>& allSettings() const;

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
    // 注册前置条件: 未封盘、id 合法(与持久化服务是否就绪无关)
    bool canRegister(const QString& pluginId, const QString& localId) const;

    QVector<SettingPageDefinition> pageDefinitions;    // 按 order 升序维护, 有新增即重排
    QVector<SettingGroupDefinition> groupDefinitions;  // 按注册顺序存放, 查询时再排序
    QVector<SettingDefinition> settingDefinitions;     // 同上
    bool sealed = false;                               // 封盘后 canRegister() 一律失败
};
