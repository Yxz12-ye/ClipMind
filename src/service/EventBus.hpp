#pragma once

#include <QDateTime>
#include <QString>
#include <QVariantMap>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

/**
 * @brief 一个已经发布或从文件加载的事件。
 */
struct EventRecord {
    QString eventType;       ///< 已注册的事件类型名称。
    QString publisherId;     ///< 原始发布者标识；加载时不自动注册该发布者。
    QDateTime timestamp;     ///< 原始发布时间；本地发布使用 UTC。
    QVariantMap parameters;  ///< 命名参数，所有嵌套对象键也必须非空。
};

/**
 * @brief 重放事件时使用的可选筛选条件。
 */
struct ReplayFilter {
    std::optional<QString> eventType;    ///< 精确匹配事件类型，未设置时匹配所有类型。
    std::optional<QString> publisherId;  ///< 精确匹配原始发布者，未设置时不过滤。
    std::optional<QDateTime> from;       ///< 包含边界的起始时间，未设置时不限制。
    std::optional<QDateTime> to;         ///< 包含边界的结束时间，未设置时不限制。
};

class EventBus;
namespace event_bus_detail {
struct EventBusState;
}

/**
 * @brief 事件类型注册表。
 *
 * 注册表只负责事件类型的生命周期和存在性校验，不负责发布、订阅或持久化。
 */
class EventTypeRegistry {
public:
    /** @brief 创建空注册表。 */
    EventTypeRegistry();
    /** @brief 释放注册表。 */
    ~EventTypeRegistry();

    EventTypeRegistry(const EventTypeRegistry&) = delete;
    EventTypeRegistry& operator=(const EventTypeRegistry&) = delete;

    /**
     * @brief 注册一个事件类型，线程安全。
     * @param eventType 精确匹配的名称，不能为空或全为空白。
     * @return 首次成功注册返回 true，重复或非法名称返回 false 并记录警告。
     */
    bool registerType(const QString& eventType);
    /**
     * @brief 判断事件类型是否已注册，线程安全。
     * @param eventType 要查询的名称。
     * @return 名称存在时返回 true。
     */
    bool contains(const QString& eventType) const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

/**
 * @brief 发布者授权句柄。
 *
 * 句柄不可复制，只能移动；句柄销毁不会注销发布者，但之后不能再通过该句柄发布事件。
 */
class PublisherHandle {
public:
    /** @brief 创建无效句柄。 */
    PublisherHandle();
    /** @brief 释放句柄，发布者标识仍保留在总线中。 */
    ~PublisherHandle();

    PublisherHandle(const PublisherHandle&) = delete;
    PublisherHandle& operator=(const PublisherHandle&) = delete;
    /** @brief 转移授权，other 随后失效。 @param other 源句柄。 */
    PublisherHandle(PublisherHandle&& other) noexcept;
    /** @brief 替换授权，other 随后失效。 @param other 源句柄。 @return 当前句柄。 */
    PublisherHandle& operator=(PublisherHandle&& other) noexcept;

    /** @brief 返回发布者标识。 @return 注册时的标识，无效句柄可能返回空串。 */
    QString publisherId() const;
    /**
     * @brief 记录事件并在调用线程同步派发。
     * @param eventType 已注册事件类型。
     * @param parameters 参数快照，键不能为空；不可 JSON 化的值可发布但不能保存。
     * @return 接受事件返回 true；句柄失效、类型未知或参数键非法返回 false。
     * @note 回调异常被捕获，不影响返回值或后续回调。相同句柄可以并发发布，
     *       但不能与移动或析构同时发生。
     */
    bool publish(const QString& eventType, const QVariantMap& parameters = {}) const;
    /** @brief 判断句柄当前是否仍可用。 @return 总线仍存在且授权有效时返回 true。 */
    bool isValid() const;

private:
    friend class EventBus;
    PublisherHandle(std::weak_ptr<event_bus_detail::EventBusState> state, QString publisherId);

    std::weak_ptr<event_bus_detail::EventBusState> state_;
    QString publisherId_;
};

/**
 * @brief 订阅生命周期令牌。
 *
 * 令牌不可复制、可移动；析构时自动取消对应订阅。
 */
class SubscriptionToken {
public:
    /** @brief 创建无效令牌。 */
    SubscriptionToken();
    /** @brief 自动取消订阅。 */
    ~SubscriptionToken();

    SubscriptionToken(const SubscriptionToken&) = delete;
    SubscriptionToken& operator=(const SubscriptionToken&) = delete;
    /** @brief 转移订阅，other 随后失效。 @param other 源令牌。 */
    SubscriptionToken(SubscriptionToken&& other) noexcept;
    /** @brief 取消旧订阅并转移新订阅。 @param other 源令牌。 @return 当前令牌。 */
    SubscriptionToken& operator=(SubscriptionToken&& other) noexcept;

    /**
     * @brief 幂等地取消订阅，不等待已取得回调快照的派发结束。
     * @note 同一个令牌不能并发取消、移动或析构；不同令牌可并发操作。
     */
    void unsubscribe();
    /** @brief 判断订阅是否仍然有效。 @return 令牌未取消且总线存在时返回 true。 */
    bool isValid() const;

private:
    friend class EventBus;
    SubscriptionToken(std::weak_ptr<event_bus_detail::EventBusState> state,
                      std::size_t subscriptionId);

    std::weak_ptr<event_bus_detail::EventBusState> state_;
    std::size_t subscriptionId_ = 0;
};

/**
 * @brief 线程安全的进程内事件总线。
 *
 * EventBus 只负责事件路由和历史记录，业务处理必须由订阅者自己的处理函数完成。
 * 所有总线方法可以并发调用，但总线析构必须发生在这些调用结束之后。
 * 回调按订阅顺序执行，不持有总线锁，可重入发布、订阅和取消订阅。
 * 不同发布线程可同时调用同一回调，订阅者必须保证自身线程安全和生命周期。
 * 取消订阅后，已经取得快照的回调仍可能执行。捕获 this 的订阅者必须先确保
 * 发布线程停止或采用弱引用，再销毁处理对象。重放使用历史快照，不重复记账。
 *
 * @code
 * class Consumer {
 * public:
 *     explicit Consumer(EventBus& bus) {
 *         token = bus.subscribe("copied", [this](const EventRecord& event) { onCopy(event); });
 *     }
 * private:
 *     void onCopy(const EventRecord& event);
 *     std::optional<SubscriptionToken> token;
 * };
 * @endcode
 */
class EventBus {
public:
    using Handler =
        std::function<void(const EventRecord&)>;  ///< 同步处理函数，引用仅在回调中有效。

    /** @brief 创建独立、空的事件总线，无全局单例。 */
    EventBus();
    /** @brief 销毁总线，使遗留的句柄与令牌失效。 */
    ~EventBus();

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /** @brief 注册事件类型。 @param eventType 非空名称。 @return 首次注册返回 true。 */
    bool registerEventType(const QString& eventType);
    /**
     * @brief 注册发布者，名称在该总线生命周期内保持唯一。
     * @param publisherId 非空发布者名称。
     * @return 授权句柄；空名称或重复注册返回 nullopt 并记录警告。
     */
    std::optional<PublisherHandle> registerPublisher(const QString& publisherId);
    /**
     * @brief 订阅已注册事件类型；调用方必须保留返回的令牌。
     * @param eventType 已注册类型。
     * @param handler 订阅者成员函数的适配回调，不能为空。
     * @return RAII 令牌；类型未知或处理函数为空时返回 nullopt 并记录警告。
     */
    std::optional<SubscriptionToken> subscribe(const QString& eventType, Handler handler);

    /**
     * @brief 用 QSaveFile 原子保存历史快照，根对象为 version:1 和 events 数组。
     * @param filePath 目标路径，父目录须已存在。
     * @return 保存成功返回 true；参数不能安全转换或 I/O 失败返回 false 并保留旧文件。
     * @note JSON 支持 null、布尔、字符串、有限数值和嵌套列表/映射。
     *       整数范围限制为 +/- (2^53-1)；Qt 自定义类型及无效 QVariant 被拒绝。
     *       JSON 加载保留值语义，不保证原来的 QVariant 数值元类型。
     */
    bool save(const QString& filePath) const;
    /**
     * @brief 校验完整文件后替换历史，不派发、不注册发布者。
     * @param filePath version:1 的 JSON 日志路径，所有事件类型须预先注册。
     * @return 成功返回 true；字段/版本/事件类型/I/O 校验失败返回 false，保留旧历史。
     * @note 与发布并发时，替换点之前的历史被覆盖，之后的发布继续追加。
     */
    bool load(const QString& filePath);
    /**
     * @brief 按原始记录顺序重放筛选后的历史快照，保留原时间及发布者。
     * @param filter 可选精确名称和包含边界的时间区间。
     * @return 重放记录数（无订阅者也计数）；无匹配或非法筛选条件返回 0。
     * @note 非法筛选条件记录警告。每条记录使用当时的订阅快照；重放不新增历史。
     */
    std::size_t replay(const ReplayFilter& filter = {});

    /** @brief 返回当前历史快照。 @return 按接受顺序排列的记录副本。 */
    std::vector<EventRecord> history() const;

private:
    friend class PublisherHandle;
    friend class SubscriptionToken;
    static bool publishForState(const std::shared_ptr<event_bus_detail::EventBusState>& state,
                                const QString& publisherId, const QString& eventType,
                                const QVariantMap& parameters);
    static void unsubscribeForState(const std::shared_ptr<event_bus_detail::EventBusState>& state,
                                    std::size_t subscriptionId);

    std::shared_ptr<event_bus_detail::EventBusState> state_;
};
