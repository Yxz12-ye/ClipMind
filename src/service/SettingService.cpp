#include "SettingService.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace {

// 配置文件的完整路径: <home><CONFIG_PATH>/<CONFIG_FILE>
QString configFilePath() {
    return QDir(QDir::homePath() + CONFIG_PATH).filePath(QStringLiteral(CONFIG_FILE));
}

// JSON 只能存布尔/数值/字符串/数组/对象, 像 QColor/QSize 这类类型无法持久化
bool isStorable(const QVariant& value) {
    const QJsonValue jsonValue = QJsonValue::fromVariant(value);
    return !jsonValue.isUndefined() && !jsonValue.isNull();
}

QString typeName(const QVariant& value) {
    const char* name = value.metaType().name();
    return name != nullptr ? QString::fromUtf8(name) : QStringLiteral("unknown");
}

}  // namespace

void SettingService::_load() {
    const QDir path = QDir(QDir::homePath() + CONFIG_PATH);
    if (!path.exists() && !QDir().mkpath(path.absolutePath())) {
        qWarning() << "create config directory failed:" << path.absolutePath();
        _isReady = false;
        return;
    }

    QFile file(configFilePath());
    if (!file.exists()) {
        // 首次运行: 以空配置启动, 注册设置时再把默认值写入文件
        _settingObj = QJsonDocument(QJsonObject{});
        _isReady = true;
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "open config file failed:" << file.fileName() << file.errorString();
        _isReady = false;
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    if (data.trimmed().isEmpty()) {
        // 空文件同样按首次运行处理
        _settingObj = QJsonDocument(QJsonObject{});
        _isReady = true;
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "config file broken!" << file.fileName() << parseError.errorString();
        _isReady = false;
        return;
    }

    _settingObj = document;
    // 重建逻辑见 _register(): 注册设置时把配置文件中缺失或无法还原的键补写回去
    _isReady = true;
    return;
}

void SettingService::_save() {
    if (!_isReady) {
        qWarning() << "setting service is not ready, save skipped";
        return;
    }

    QJsonObject root = _settingObj.object();
    for (auto it = _values.constBegin(); it != _values.constEnd(); ++it) {
        const QVariant& value = it.value();
        const QJsonValue jsonValue = QJsonValue::fromVariant(value);
        if (!value.isValid() || jsonValue.isUndefined() || jsonValue.isNull()) {
            qWarning() << "setting cannot be stored as json:" << it.key();
            continue;
        }

        root.insert(it.key(), jsonValue);
    }
    _settingObj.setObject(root);

    QSaveFile file(configFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "open config file for writing failed:" << file.fileName()
                   << file.errorString();
        return;
    }

    if (file.write(_settingObj.toJson(QJsonDocument::Indented)) == -1 || !file.commit()) {
        qWarning() << "save config file failed:" << file.fileName() << file.errorString();
        return;
    }

    _dirty = false;
}

void SettingService::_read(const QString& key) {
    const QVariant defaultValue = _defaultValues.value(key);
    const QJsonValue stored = _settingObj.object().value(key);

    // JSON 只能区分字符串/数值/布尔/数组/对象, 需要按注册时记录的类型还原
    bool restored = false;
    QVariant value;
    if (!stored.isUndefined() && !stored.isNull() && defaultValue.isValid()) {
        value = stored.toVariant();
        restored =
            value.metaType() == defaultValue.metaType() ||
            (value.canConvert(defaultValue.metaType()) && value.convert(defaultValue.metaType()));
    }

    if (!restored) {
        if (!stored.isUndefined()) {
            qWarning() << "unable to restore setting:" << key << "fallback to default value";
        }

        _values.insert(key, defaultValue);
        _dirty = true;  // 该键缺失或无法还原, 需要把默认值写回配置文件
        return;
    }

    _values.insert(key, value);
}

bool SettingService::_register(const QString& key, const QVariant& defaultValue) {
    if (key.isEmpty()) {
        qWarning() << "setting key is empty";
        return false;
    }

    if (_defaultValues.contains(key)) {
        qWarning() << "setting already registered:" << key;
        return false;
    }

    if (!_isReady) {
        qWarning() << "setting service is not ready, register failed:" << key;
        return false;
    }

    if (!isStorable(defaultValue)) {
        qWarning() << "setting default value cannot be stored as json:" << key;
        return false;
    }

    _defaultValues.insert(key, defaultValue);
    _read(key);
    if (_dirty) {
        _save();
    }

    return true;
}

bool SettingService::_set(const QString& key, const QVariant& value) {
    if (!_isReady) {
        qWarning() << "setting service is not ready, set failed:" << key;
        return false;
    }

    if (!_defaultValues.contains(key)) {
        qWarning() << "setting is not registered:" << key;
        return false;
    }

    // 按注册时的类型还原, 保证存进文件的类型始终和注册的一致
    const QVariant& defaultValue = _defaultValues.value(key);
    QVariant converted = value;
    if (converted.metaType() != defaultValue.metaType() &&
        (!converted.canConvert(defaultValue.metaType()) ||
         !converted.convert(defaultValue.metaType()))) {
        qWarning() << "setting type mismatch:" << key << typeName(converted) << "->"
                   << typeName(defaultValue);
        return false;
    }

    if (!isStorable(converted)) {
        qWarning() << "setting cannot be stored as json:" << key;
        return false;
    }

    _values.insert(key, converted);
    _dirty = true;
    _save();
    emit valueChanged(key, converted);

    return !_dirty;  // 落盘失败时 _dirty 仍为 true
}

bool SettingService::registerSetting(const QString& key, const QVariant& defaultValue) {
    return _register(key, defaultValue);
}

SettingService::SettingService(/* args */) {
    _load();
}

SettingService::~SettingService() {
    if (_dirty) {
        _save();  // 上次写入失败时再尝试落盘
    }
}

SettingService* SettingService::instance() {
    static SettingService* service_ptr = nullptr;
    if (service_ptr == nullptr) {
        service_ptr = new SettingService();
    }
    return service_ptr;
}

bool SettingService::ready() {
    return _isReady;
}

QVariant SettingService::get(const QString& key) const {
    if (!_values.contains(key)) {
        qWarning() << "setting is not registered:" << key;
        return {};
    }

    return _values.value(key);
}

bool SettingService::set(const QString& key, const QVariant& value) {
    return _set(key, value);
}
