#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QVariant>
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
class SettingService : public QObject {
    Q_OBJECT
private:
    void _load();  // 加载配置并生成QJsonDocument

    void _save();                    // 保存配置文件
    void _read(const QString& key);  // 按注册时的类型还原单个键, 缺失或无法还原时回退到默认值

    bool _register(const QString& key, const QVariant& defaultValue);  // registerSetting 的实现
    bool _set(const QString& key, const QVariant& value);              // set 的实现

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
     */
    template <typename T>
    bool registerSetting(const QString& key, const T& defaultValue) {
        return _register(key, _toVariant(defaultValue));
    }
    bool registerSetting(const QString& key, const QVariant& defaultValue);
    /**
     * @brief 修改已注册的键的值, 类型需与注册时的默认值兼容, 成功后立即写回配置文件
     * @param key 唯一键值
     * @param value 新值, 可类型推导
     * @return 修改并落盘成功返回`true`; 写盘失败时值已在内存中生效, 返回`false`
     */
    template <typename T>
    bool set(const QString& key, const T& value) {
        return _set(key, _toVariant(value));
    }
    /**
     * @brief 以 `QVariant` 形式修改已注册的键的值
     * @param key 唯一键值
     * @param value 新值
     * @return 修改并落盘成功返回`true`; 写盘失败时值已在内存中生效, 返回`false`
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
