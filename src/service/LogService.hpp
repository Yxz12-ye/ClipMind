#pragma once

#include <QByteArray>
#include <QString>
#include <cstddef>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <string_view>
#include <utility>

// ---------------------------------------------------------------------------
// fmt 不认识 QString, 这里补一个格式化器, 让日志调用可以直接写成:
//     LogService::warn("SQLService", "prepare failed: {}", lastError());
// 日志统一按 UTF-8 落盘, 不依赖运行环境的本地编码
// ---------------------------------------------------------------------------
template <>
struct fmt::formatter<QString> {
    constexpr auto parse(fmt::format_parse_context& context) { return context.begin(); }

    template <typename FormatContext>
    auto format(const QString& value, FormatContext& context) const {
        const QByteArray utf8 = value.toUtf8();
        return fmt::format_to(
            context.out(), "{}",
            std::string_view(utf8.constData(), static_cast<std::size_t>(utf8.size())));
    }
};

// ---------------------------------------------------------------------------
// 进程内唯一的日志入口, 底层是 spdlog。
//
// 输出格式: [时间] [等级] [模块]:日志内容
//     [2025-01-01 12:00:00.123] [WARN] [SQLService]:prepare get failed: no such table
//
// 日志文件: 系统临时目录 <temp>/ClipMind/ClipMind.log, 按大小轮转保留若干份
//
// 记录范围: Debug 构建记录全部日志, Release 构建只记录关键日志(WARN 及以上),
//           下限由编译期的 kMinimumLevel 决定, Release 下运行期也无法调低。
// ---------------------------------------------------------------------------
namespace LogService {

// 日志级别, 数值与 spdlog 保持一致: 越大越严重
enum class Level { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, Critical = 5 };

// 编译期级别下限: Debug 构建(未定义 NDEBUG)记录全部, Release 构建(定义了 NDEBUG)只记录关键日志
#ifdef NDEBUG
inline constexpr Level kMinimumLevel = Level::Warn;
#else
inline constexpr Level kMinimumLevel = Level::Trace;
#endif

// 启动日志: 创建日志目录与文件, 并把 Qt 自身的日志接到同一份文件里。
// 幂等, 可重复调用; 失败(例如临时目录不可写)时返回 false, 此时只保留控制台输出
bool initialize();

// 冲刷并关闭日志; 进程退出前调用, 之后不再接受任何日志
void shutdown();

bool initialized();

// 日志文件的完整路径, 未初始化(或文件不可写)时返回空字符串
QString logFilePath();

// 运行期调整记录级别, 但不会低于编译期下限(Release 下依旧是关键日志起步)
void setLevel(Level target);
Level level();

namespace detail {

// 判断该级别是否需要记录: 先判级别再格式化, 被过滤掉的日志不产生任何字符串构造
bool shouldLog(Level target);

// 落盘入口: message 必须是已格式化好的文本, 不会再被当作格式串解释
void write(Level target, std::string_view module, std::string_view message);

}  // namespace detail

template <typename... Args>
void log(Level target, std::string_view module, spdlog::format_string_t<Args...> format,
         Args&&... args) {
    if (detail::shouldLog(target)) {
        detail::write(target, module, spdlog::fmt_lib::format(format, std::forward<Args>(args)...));
    }
}

template <typename... Args>
void trace(std::string_view module, spdlog::format_string_t<Args...> format, Args&&... args) {
    log(Level::Trace, module, format, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(std::string_view module, spdlog::format_string_t<Args...> format, Args&&... args) {
    log(Level::Debug, module, format, std::forward<Args>(args)...);
}

template <typename... Args>
void info(std::string_view module, spdlog::format_string_t<Args...> format, Args&&... args) {
    log(Level::Info, module, format, std::forward<Args>(args)...);
}

template <typename... Args>
void warn(std::string_view module, spdlog::format_string_t<Args...> format, Args&&... args) {
    log(Level::Warn, module, format, std::forward<Args>(args)...);
}

template <typename... Args>
void error(std::string_view module, spdlog::format_string_t<Args...> format, Args&&... args) {
    log(Level::Error, module, format, std::forward<Args>(args)...);
}

template <typename... Args>
void critical(std::string_view module, spdlog::format_string_t<Args...> format, Args&&... args) {
    log(Level::Critical, module, format, std::forward<Args>(args)...);
}

}  // namespace LogService
