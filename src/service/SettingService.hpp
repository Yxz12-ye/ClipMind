#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVariant>
#include <QVector>
#include <type_traits>

#ifdef Q_OS_WIN
#define CONFIG_PATH "/AppData/Local/ClipMind"
#elif defined(Q_OS_LINUX)
#define CONFIG_PATH "/.local/share/ClipMind"  // 或 "/.config/ClipMind"
#elif defined(Q_OS_MACOS)
#define CONFIG_PATH "/Library/Application Support/ClipMind"
#endif
#define CONFIG_FILE "config.json"

// 使用Json存储用户设置
// 注册为只读的项(固定文本, 见 registerSetting() 的 persistent 参数)只留在内存中, 不写配置文件
class SettingService : public QObject {
    Q_OBJECT
private:
    void _load();  // 加载配置并生成QJsonDocument

    void _save();  // 保存配置文件
    // 读取单个键: 按注册时的类型还原, 缺失或无法还原时回退到默认值
    void _read(const QString& key, bool persistent);
    // registerSetting 的实现(只改内存): persistent 为 false 时不落盘, 并登记为只读
    bool _register(const QString& key, const QVariant& defaultValue, bool persistent);
    bool _set(const QString& key, const QVariant& value);  // set 的实现

    // 把默认值/新值包装成 QVariant, 可隐式转为 QString 的类型统一按字符串存储
    template <typename T>
    static QVariant _toVariant(const T& value) {
        if constexpr (std::is_convertible_v<std::decay_t<T>, QString>) {
            return QVariant::fromValue(QString(value));
        } else {
            return QVariant::fromValue(value);
        }
    }

    QJsonDocument _settingObj;
    QHash<QString, QVariant> _defaultValues;  // 键 -> 注册时的默认值(同时记录类型)
    QHash<QString, QVariant> _values;         // 键 -> 当前值
    QSet<QString> _fixedKeys;                 // 只读键(固定文本项), 永不写入配置文件
    bool _dirty = false;                      // 有键尚未写回配置文件
    bool _isReady = false;

public:
    SettingService(/* args */);
    ~SettingService();

    static SettingService* instance();

    bool ready();
    /**
     * @brief 需要持久化的设置可通过调用该函数去注册持久化
     * @param key 唯一键值
     * @param defaultValue 默认值, 可类型推导
     * @return 注册成功返回`true`, 其他情况返回`false`
     *
     * 每次调用都会在需要时把配置文件整体写回一遍, 批量注册请用 registerSettings()。
     */
    template <typename T>
    bool registerSetting(const QString& key, const T& defaultValue) {
        return registerSetting(key, _toVariant(defaultValue), true);
    }
    bool registerSetting(const QString& key, const QVariant& defaultValue);
    /**
     * @brief 注册一个设置项, 由 `persistent` 决定它是否需要写进配置文件
     * @param key 唯一键值
     * @param value 注册时的值, 会作为该项的默认值
     * @param persistent 为 `false` 时只存在内存中, 永不写盘, 且该项变成只读(set() 会被拒绝)
     * @return 注册成功返回`true`, 其他情况返回`false`
     *
     * `persistent == false` 用于"固定文本"这类只展示给用户看的信息(例如软件版本号):
     * 它不属于用户偏好, 配置文件里出现同名旧键也会被忽略, 一律以本次注册的值为准。
     */
    bool registerSetting(const QString& key, const QVariant& value, bool persistent);
    /**
     * @brief 批量注册设置项, 语义与逐条 registerSetting() 相同, 但只在整个批次结束时落盘一次
     * @param defaultValues 键与默认值列表, 按给定顺序注册
     * @param persistent 含义同 registerSetting(); 为 `false` 时整批都只留在内存中, 不产生任何写入
     * @return 全部注册成功返回`true`; 任一项被拒绝时返回`false`(被拒绝的项会单独打印警告,
     *         其余项仍然注册成功), 落盘失败不体现在返回值里
     */
    bool registerSettings(const QVector<QPair<QString, QVariant>>& defaultValues,
                          bool persistent = true);
    /**
     * @brief 修改已注册的键的值, 类型需与注册时的默认值兼容, 成功后立即写回配置文件
     * @param key 唯一键值
     * @param value 新值, 可类型推导
     * @return 修改并落盘成功返回`true`; 只读项(注册时 persistent 为 `false`)与写盘失败时返回`false`
     *
     * 注意写盘失败时值已在内存中生效, 只读项则是原值保持不变。
     */
    template <typename T>
    bool set(const QString& key, const T& value) {
        return _set(key, _toVariant(value));
    }
    /**
     * @brief 以 `QVariant` 形式修改已注册的键的值
     * @param key 唯一键值
     * @param value 新值
     * @return 修改并落盘成功返回`true`; 只读项与写盘失败时返回`false`
     */
    bool set(const QString& key, const QVariant& value);
    /**
     * @brief 获取已注册的键对应的值
     * @param key 唯一键值
     * @return 返回`QVariant`对象, 由调用方决定转化
     */
    QVariant get(const QString& key) const;

signals:
    void valueChanged(const QString& key, const QVariant& value);
};
