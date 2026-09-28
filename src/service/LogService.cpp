#include "LogService.hpp"

#include <QDebug>
#include <QDir>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>
#include <vector>

namespace {

// 日志文件: <系统临时目录>/ClipMind/ClipMind.log
constexpr std::size_t kMaxFileSize = 5 * 1024 * 1024;  // 单个文件上限 5MB
constexpr std::size_t kMaxFileCount = 3;               // 轮转保留的文件数(含当前文件)

std::mutex g_mutex;
std::shared_ptr<spdlog::logger> g_logger;
QString g_logFilePath;
std::atomic<int> g_level{static_cast<int>(LogService::kMinimumLevel)};
std::atomic<bool> g_terminated{false};

// spdlog 内置的 %l 是小写, 且 warn 会写成 warning, 这里统一成大写短名
std::string_view levelLabel(spdlog::level::level_enum value) {
    switch (value) {
    case spdlog::level::trace:
        return "TRACE";
    case spdlog::level::debug:
        return "DEBUG";
    case spdlog::level::info:
        return "INFO";
    case spdlog::level::warn:
        return "WARN";
    case spdlog::level::err:
        return "ERROR";
    case spdlog::level::critical:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

// 自定义格式标记 %*: 输出大写级别名
class LevelFlag final : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& message, const std::tm& /*time*/,
                spdlog::memory_buf_t& destination) override {
        const std::string_view label = levelLabel(message.level);
        destination.append(label.data(), label.data() + label.size());
    }

    std::unique_ptr<spdlog::custom_flag_formatter> clone() const override {
        return std::make_unique<LevelFlag>();
    }
};

// 输出格式: [时间] [等级] [模块]:日志内容
// 其中 "[模块]:" 与内容由 detail::write() 拼进 %v, %^/%$ 只给控制台着色用
std::unique_ptr<spdlog::formatter> makeFormatter() {
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<LevelFlag>('*');
    formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %^[%*]%$ %v");
    return formatter;
}

// spdlog 在 Windows 上用窄字符路径 fopen, 这里先转成本地编码,
// 否则临时目录里出现中文用户名时文件会打不开; 其他平台统一用 UTF-8
std::string nativePath(const QString& path) {
#ifdef Q_OS_WIN
    const QByteArray bytes = path.toLocal8Bit();
#else
    const QByteArray bytes = path.toUtf8();
#endif
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

LogService::Level levelOf(QtMsgType type) {
    switch (type) {
    case QtDebugMsg:
        return LogService::Level::Debug;
    case QtInfoMsg:
        return LogService::Level::Info;
    case QtWarningMsg:
        return LogService::Level::Warn;
    case QtCriticalMsg:
        return LogService::Level::Error;
    case QtFatalMsg:
        return LogService::Level::Critical;
    }

    return LogService::Level::Info;
}

// Qt 自身的输出也接进同一份日志, 模块名统一记为 Qt
void qtMessageHandler(QtMsgType type, const QMessageLogContext& /*context*/,
                      const QString& message) {
    LogService::log(levelOf(type), "Qt", "{}", message);
}

// 调用前需持有 g_mutex
bool createLogger() {
    if (g_logger != nullptr || g_terminated.load(std::memory_order_relaxed)) {
        return g_logger != nullptr;
    }

    const QString directory = QDir(QDir::tempPath()).filePath(QStringLiteral("ClipMind"));
    if (!QDir().mkpath(directory)) {
        return false;
    }

    const QString logPath = QDir(directory).filePath(QStringLiteral("ClipMind.log"));

    std::vector<spdlog::sink_ptr> sinks;

    bool fileSinkReady = false;
    try {
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            nativePath(logPath), kMaxFileSize, kMaxFileCount);
        fileSink->set_level(spdlog::level::trace);  // 级别统一由 LogService 过滤
        sinks.push_back(std::move(fileSink));
        fileSinkReady = true;
    } catch (const std::exception&) {
        // 临时目录不可写时退化为只写控制台: 记日志失败不能影响主流程
    }

#ifndef NDEBUG
    // Debug 构建额外输出到控制台, 方便直接在终端里看
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::trace);
    sinks.push_back(std::move(consoleSink));
#endif

    if (sinks.empty()) {
        return false;
    }

    auto logger = std::make_shared<spdlog::logger>("ClipMind", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);  // 真正的过滤在 detail::shouldLog() 里
    logger->set_formatter(makeFormatter());
    logger->set_error_handler([](const std::string& /*error*/) {
        // 日志写失败无处可报, 直接丢弃
    });

    g_logger = std::move(logger);
    g_logFilePath = fileSinkReady ? logPath : QString();
    return true;
}

}  // namespace

namespace LogService {

namespace detail {

bool shouldLog(Level target) {
    return static_cast<int>(target) >= g_level.load(std::memory_order_relaxed);
}

void write(Level target, std::string_view module, std::string_view message) {
    if (g_terminated.load(std::memory_order_relaxed)) {
        return;
    }

    std::shared_ptr<spdlog::logger> logger;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        logger = g_logger;
    }

    if (logger == nullptr) {
        // 第一条日志早于 initialize() 时补一次初始化, 避免日志丢失
        initialize();

        std::lock_guard<std::mutex> lock(g_mutex);
        logger = g_logger;
    }

    if (logger == nullptr) {
        return;
    }

    // 内容不参与格式化, 避免日志文本里的花括号被当成格式串
    const std::string line = spdlog::fmt_lib::format("[{}]:{}", module, message);
    logger->log(static_cast<spdlog::level::level_enum>(target),
                spdlog::string_view_t(line.data(), line.size()));
}

}  // namespace detail

bool initialize() {
    bool ready = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        ready = createLogger();
    }

    if (ready) {
        // 在锁外安装: Qt 消息处理器会回调 write(), 拿锁时不能已经持有 g_mutex
        qInstallMessageHandler(qtMessageHandler);
    }

    return ready;
}

void shutdown() {
    qInstallMessageHandler(nullptr);

    std::shared_ptr<spdlog::logger> logger;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        logger = std::move(g_logger);
        g_logger = nullptr;
        g_terminated.store(true, std::memory_order_relaxed);
    }

    if (logger != nullptr) {
        logger->flush();
    }
}

bool initialized() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_logger != nullptr;
}

QString logFilePath() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_logFilePath;
}

void setLevel(Level target) {
    // 不允许低于编译期下限: Release 构建即使调用也只会记录关键日志
    const int clamped = std::max(static_cast<int>(target), static_cast<int>(kMinimumLevel));
    g_level.store(clamped, std::memory_order_relaxed);
}

Level level() {
    return static_cast<Level>(g_level.load(std::memory_order_relaxed));
}

}  // namespace LogService
