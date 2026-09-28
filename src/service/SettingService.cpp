#include "SettingService.hpp"

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include "LogService.hpp"

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
        LogService::warn("SettingService", "create config directory failed: {}",
                         path.absolutePath());
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
        LogService::warn("SettingService", "open config file failed: {} {}", file.fileName(),
                         file.errorString());
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
        LogService::warn("SettingService", "config file broken! {} {}", file.fileName(),
                         parseError.errorString());
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
        LogService::warn("SettingService", "setting service is not ready, save skipped");
        return;
    }

    QJsonObject root = _settingObj.object();
    for (auto it = _values.constBegin(); it != _values.constEnd(); ++it) {
        const QVariant& value = it.value();
        const QJsonValue jsonValue = QJsonValue::fromVariant(value);
        if (!value.isValid() || jsonValue.isUndefined() || jsonValue.isNull()) {
            LogService::warn("SettingService", "setting cannot be stored as json: {}", it.key());
            continue;
        }

        root.insert(it.key(), jsonValue);
    }
    _settingObj.setObject(root);

    QSaveFile file(configFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        LogService::warn("SettingService", "open config file for writing failed: {} {}",
                         file.fileName(), file.errorString());
        return;
    }

    if (file.write(_settingObj.toJson(QJsonDocument::Indented)) == -1 || !file.commit()) {
        LogService::warn("SettingService", "save config file failed: {} {}", file.fileName(),
                         file.errorString());
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
            LogService::warn("SettingService",
                             "unable to restore setting: {} fallback to default value", key);
        }

        _values.insert(key, defaultValue);
        _dirty = true;  // 该键缺失或无法还原, 需要把默认值写回配置文件
        return;
    }

    _values.insert(key, value);
}

bool SettingService::_register(const QString& key, const QVariant& defaultValue) {
    if (key.isEmpty()) {
        LogService::warn("SettingService", "setting key is empty");
        return false;
    }

    if (_defaultValues.contains(key)) {
        LogService::warn("SettingService", "setting already registered: {}", key);
        return false;
    }

    if (!_isReady) {
        LogService::warn("SettingService", "setting service is not ready, register failed: {}",
                         key);
        return false;
    }

    if (!isStorable(defaultValue)) {
        LogService::warn("SettingService", "setting default value cannot be stored as json: {}",
                         key);
        return false;
    }

    _defaultValues.insert(key, defaultValue);
    _read(key);  // 缺失或无法还原时会置 _dirty, 由调用方决定何时落盘
    return true;
}

bool SettingService::_set(const QString& key, const QVariant& value) {
    if (!_isReady) {
        LogService::warn("SettingService", "setting service is not ready, set failed: {}", key);
        return false;
    }

    if (!_defaultValues.contains(key)) {
        LogService::warn("SettingService", "setting is not registered: {}", key);
        return false;
    }

    // 按注册时的类型还原, 保证存进文件的类型始终和注册的一致
    const QVariant& defaultValue = _defaultValues.value(key);
    QVariant converted = value;
    if (converted.metaType() != defaultValue.metaType() &&
        (!converted.canConvert(defaultValue.metaType()) ||
         !converted.convert(defaultValue.metaType()))) {
        LogService::warn("SettingService", "setting type mismatch: {} {} -> {}", key,
                         typeName(converted), typeName(defaultValue));
        return false;
    }

    if (!isStorable(converted)) {
        LogService::warn("SettingService", "setting cannot be stored as json: {}", key);
        return false;
    }

    _values.insert(key, converted);
    _dirty = true;
    _save();
    emit valueChanged(key, converted);

    return !_dirty;  // 落盘失败时 _dirty 仍为 true
}

bool SettingService::registerSetting(const QString& key, const QVariant& defaultValue) {
    if (!_register(key, defaultValue)) {
        return false;
    }

    if (_dirty) {
        _save();
    }

    return true;
}

bool SettingService::registerSettings(const QVector<QPair<QString, QVariant>>& defaultValues) {
    bool allRegistered = true;
    for (const auto& [key, defaultValue] : defaultValues) {
        if (!_register(key, defaultValue)) {
            allRegistered = false;
        }
    }

    // 整批只落盘一次: 首次运行时缺键较多, 逐条注册会把配置文件全量重写很多遍
    if (_dirty) {
        _save();
    }

    return allRegistered;
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
        LogService::warn("SettingService", "setting is not registered: {}", key);
        return {};
    }

    return _values.value(key);
}

bool SettingService::set(const QString& key, const QVariant& value) {
    return _set(key, value);
}
