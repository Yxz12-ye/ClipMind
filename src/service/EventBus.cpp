#include "EventBus.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <mutex>
#include <utility>

#include "LogService.hpp"

namespace {

bool validName(const QString& value) {
    return !value.trimmed().isEmpty();
}

bool failure(const char* message) {
    LogService::warn("EventBus", "{}", message);
    return false;
}

bool validParameterKeys(const QVariant& value) {
    if (value.metaType().id() == QMetaType::QVariantMap) {
        const auto map = value.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            if (!validName(it.key()) || !validParameterKeys(it.value())) {
                return false;
            }
        }
    } else if (value.metaType().id() == QMetaType::QVariantList) {
        for (const auto& item : value.toList()) {
            if (!validParameterKeys(item)) {
                return false;
            }
        }
    }
    return true;
}

// Qt silently turns unsupported leaves into null; validate nested values before conversion.
bool jsonCompatible(const QVariant& value) {
    switch (value.metaType().id()) {
    case QMetaType::Nullptr:
    case QMetaType::Bool:
    case QMetaType::QString:
    case QMetaType::QStringList:
    case QMetaType::Int:
    case QMetaType::UInt:
        return true;
    case QMetaType::LongLong:
        return value.toLongLong() >= -9007199254740991LL &&
               value.toLongLong() <= 9007199254740991LL;
    case QMetaType::ULongLong:
        return value.toULongLong() <= 9007199254740991ULL;
    case QMetaType::Double:
    case QMetaType::Float:
        return std::isfinite(value.toDouble());
    case QMetaType::QVariantMap: {
        const auto map = value.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            if (!validName(it.key()) || !jsonCompatible(it.value())) {
                return false;
            }
        }
        return true;
    }
    case QMetaType::QVariantList:
        for (const auto& item : value.toList()) {
            if (!jsonCompatible(item)) {
                return false;
            }
        }
        return true;
    default:
        return false;
    }
}

void dispatch(const std::vector<std::shared_ptr<EventBus::Handler>>& handlers,
              const EventRecord& record) {
    for (const auto& handler : handlers) {
        try {
            (*handler)(record);
        } catch (const std::exception& exception) {
            LogService::error("EventBus", "event handler threw: {}", exception.what());
        } catch (...) {
            LogService::error("EventBus", "event handler threw an unknown exception");
        }
    }
}

bool recordToJson(const EventRecord& record, QJsonObject* object) {
    if (object == nullptr || !validName(record.eventType) || !validName(record.publisherId) ||
        !record.timestamp.isValid()) {
        return false;
    }
    if (!jsonCompatible(record.parameters)) {
        return false;
    }
    const QJsonValue parameters = QJsonValue::fromVariant(record.parameters);
    if (parameters.isUndefined() || parameters.isNull() || !parameters.isObject()) {
        return false;
    }
    object->insert(QStringLiteral("eventType"), record.eventType);
    object->insert(QStringLiteral("publisherId"), record.publisherId);
    object->insert(QStringLiteral("timestamp"), record.timestamp.toString(Qt::ISODateWithMs));
    object->insert(QStringLiteral("parameters"), parameters);
    return true;
}

bool recordFromJson(const QJsonValue& value, EventRecord* record) {
    if (record == nullptr || !value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue eventType = object.value(QStringLiteral("eventType"));
    const QJsonValue publisherId = object.value(QStringLiteral("publisherId"));
    const QJsonValue timestamp = object.value(QStringLiteral("timestamp"));
    const QJsonValue parameters = object.value(QStringLiteral("parameters"));
    if (!eventType.isString() || !publisherId.isString() || !timestamp.isString() ||
        !parameters.isObject()) {
        return false;
    }
    const QDateTime parsedTime = QDateTime::fromString(timestamp.toString(), Qt::ISODateWithMs);
    if (!validName(eventType.toString()) || !validName(publisherId.toString()) ||
        !parsedTime.isValid()) {
        return false;
    }
    record->eventType = eventType.toString();
    record->publisherId = publisherId.toString();
    record->timestamp = parsedTime;
    record->parameters = parameters.toObject().toVariantMap();
    return validParameterKeys(record->parameters);
}

}  // namespace

struct EventTypeRegistry::State {
    mutable std::mutex mutex;
    QSet<QString> types;
};

EventTypeRegistry::EventTypeRegistry() : state_(std::make_unique<State>()) {}
EventTypeRegistry::~EventTypeRegistry() = default;

bool EventTypeRegistry::registerType(const QString& eventType) {
    if (!validName(eventType)) {
        return failure("event type is empty");
    }
    std::lock_guard lock(state_->mutex);
    if (state_->types.contains(eventType)) {
        return failure("event type is already registered");
    }
    state_->types.insert(eventType);
    return true;
}

bool EventTypeRegistry::contains(const QString& eventType) const {
    std::lock_guard lock(state_->mutex);
    return state_->types.contains(eventType);
}

struct event_bus_detail::EventBusState {
    struct Subscription {
        QString eventType;
        std::shared_ptr<EventBus::Handler> handler;
    };

    mutable std::mutex mutex;
    EventTypeRegistry registry;
    QSet<QString> publishers;
    std::map<std::size_t, Subscription> subscriptions;
    std::vector<EventRecord> history;
    std::size_t nextSubscriptionId = 1;
};

PublisherHandle::PublisherHandle() = default;
PublisherHandle::~PublisherHandle() = default;
PublisherHandle::PublisherHandle(std::weak_ptr<event_bus_detail::EventBusState> state,
                                 QString publisherId)
    : state_(std::move(state)), publisherId_(std::move(publisherId)) {}
PublisherHandle::PublisherHandle(PublisherHandle&& other) noexcept
    : state_(std::move(other.state_)), publisherId_(std::move(other.publisherId_)) {}
PublisherHandle& PublisherHandle::operator=(PublisherHandle&& other) noexcept {
    if (this != &other) {
        state_ = std::move(other.state_);
        publisherId_ = std::move(other.publisherId_);
    }
    return *this;
}
QString PublisherHandle::publisherId() const {
    return publisherId_;
}
bool PublisherHandle::isValid() const {
    return !state_.expired() && validName(publisherId_);
}
bool PublisherHandle::publish(const QString& eventType, const QVariantMap& parameters) const {
    const auto state = state_.lock();
    if (!state) {
        return failure("publisher handle is invalid or bus has expired");
    }
    return EventBus::publishForState(state, publisherId_, eventType, parameters);
}

SubscriptionToken::SubscriptionToken() = default;
SubscriptionToken::~SubscriptionToken() {
    unsubscribe();
}
SubscriptionToken::SubscriptionToken(std::weak_ptr<event_bus_detail::EventBusState> state,
                                     std::size_t subscriptionId)
    : state_(std::move(state)), subscriptionId_(subscriptionId) {}
SubscriptionToken::SubscriptionToken(SubscriptionToken&& other) noexcept
    : state_(std::move(other.state_)), subscriptionId_(other.subscriptionId_) {
    other.subscriptionId_ = 0;
}
SubscriptionToken& SubscriptionToken::operator=(SubscriptionToken&& other) noexcept {
    if (this != &other) {
        unsubscribe();
        state_ = std::move(other.state_);
        subscriptionId_ = other.subscriptionId_;
        other.subscriptionId_ = 0;
    }
    return *this;
}
void SubscriptionToken::unsubscribe() {
    const auto state = state_.lock();
    if (state != nullptr && subscriptionId_ != 0) {
        EventBus::unsubscribeForState(state, subscriptionId_);
    }
    state_.reset();
    subscriptionId_ = 0;
}
bool SubscriptionToken::isValid() const {
    return subscriptionId_ != 0 && !state_.expired();
}

EventBus::EventBus() : state_(std::make_shared<event_bus_detail::EventBusState>()) {}
EventBus::~EventBus() = default;

bool EventBus::registerEventType(const QString& eventType) {
    return state_->registry.registerType(eventType);
}

std::optional<PublisherHandle> EventBus::registerPublisher(const QString& publisherId) {
    if (!validName(publisherId)) {
        failure("publisher id is empty");
        return std::nullopt;
    }
    std::lock_guard lock(state_->mutex);
    if (state_->publishers.contains(publisherId)) {
        failure("publisher is already registered");
        return std::nullopt;
    }
    state_->publishers.insert(publisherId);
    return PublisherHandle(state_, publisherId);
}

std::optional<SubscriptionToken> EventBus::subscribe(const QString& eventType, Handler handler) {
    if (!state_->registry.contains(eventType) || !handler) {
        failure("subscription requires a registered event type and a handler");
        return std::nullopt;
    }
    auto callback = std::make_shared<Handler>(std::move(handler));
    std::lock_guard lock(state_->mutex);
    if (state_->nextSubscriptionId == std::numeric_limits<std::size_t>::max()) {
        failure("subscription id limit reached");
        return std::nullopt;
    }
    const std::size_t id = state_->nextSubscriptionId++;
    state_->subscriptions.emplace(
        id, event_bus_detail::EventBusState::Subscription{eventType, std::move(callback)});
    return SubscriptionToken(state_, id);
}

bool EventBus::publishForState(const std::shared_ptr<event_bus_detail::EventBusState>& state,
                               const QString& publisherId, const QString& eventType,
                               const QVariantMap& parameters) {
    if (state == nullptr || !state->registry.contains(eventType) ||
        !validParameterKeys(parameters)) {
        return failure("publish requires a registered type and nonempty parameter keys");
    }
    std::vector<std::shared_ptr<Handler>> handlers;
    EventRecord record{eventType, publisherId, {}, parameters};
    {
        std::lock_guard lock(state->mutex);
        if (!state->publishers.contains(publisherId)) {
            return failure("publisher is not registered");
        }
        record.timestamp = QDateTime::currentDateTimeUtc();
        state->history.push_back(record);
        for (const auto& [id, subscription] : state->subscriptions) {
            Q_UNUSED(id);
            if (subscription.eventType == eventType) {
                handlers.push_back(subscription.handler);
            }
        }
    }
    dispatch(handlers, record);
    return true;
}

void EventBus::unsubscribeForState(const std::shared_ptr<event_bus_detail::EventBusState>& state,
                                   std::size_t subscriptionId) {
    if (state == nullptr) {
        return;
    }
    decltype(state->subscriptions)::node_type removed;
    {
        std::lock_guard lock(state->mutex);
        removed = state->subscriptions.extract(subscriptionId);
    }
}

bool EventBus::save(const QString& filePath) const {
    if (!validName(filePath)) {
        return failure("save path is empty");
    }
    QJsonArray events;
    {
        const auto records = history();
        for (const EventRecord& record : records) {
            QJsonObject object;
            if (!recordToJson(record, &object)) {
                return failure("event parameters cannot be saved as JSON");
            }
            events.append(object);
        }
    }
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("events"), events}};
    const QByteArray bytes = QJsonDocument(root).toJson();
    QSaveFile file(filePath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        return failure("unable to atomically save event history");
    }
    return true;
}

bool EventBus::load(const QString& filePath) {
    if (!validName(filePath)) {
        return failure("load path is empty");
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure("unable to open event history");
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return failure("unable to read event history");
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return failure("invalid event history JSON");
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")) != QJsonValue(1) ||
        !root.value(QStringLiteral("events")).isArray()) {
        return failure("unsupported event history version or missing events array");
    }
    std::vector<EventRecord> loaded;
    for (const QJsonValue& value : root.value(QStringLiteral("events")).toArray()) {
        EventRecord record;
        if (!recordFromJson(value, &record) || !state_->registry.contains(record.eventType)) {
            return failure("invalid event record or unregistered event type");
        }
        loaded.push_back(std::move(record));
    }
    {
        std::lock_guard lock(state_->mutex);
        state_->history = std::move(loaded);
    }
    return true;
}

std::size_t EventBus::replay(const ReplayFilter& filter) {
    if ((filter.eventType && !state_->registry.contains(*filter.eventType)) ||
        (filter.publisherId && !validName(*filter.publisherId)) ||
        (filter.from && !filter.from->isValid()) || (filter.to && !filter.to->isValid()) ||
        (filter.from && filter.to && *filter.from > *filter.to)) {
        failure("invalid replay filter");
        return 0;
    }
    std::vector<EventRecord> records;
    {
        std::lock_guard lock(state_->mutex);
        for (const EventRecord& record : state_->history) {
            if (filter.eventType && record.eventType != *filter.eventType)
                continue;
            if (filter.publisherId && record.publisherId != *filter.publisherId)
                continue;
            if (filter.from && record.timestamp < *filter.from)
                continue;
            if (filter.to && record.timestamp > *filter.to)
                continue;
            records.push_back(record);
        }
    }
    std::size_t replayed = 0;
    for (const EventRecord& record : records) {
        std::vector<std::shared_ptr<Handler>> handlers;
        {
            std::lock_guard lock(state_->mutex);
            for (const auto& [id, subscription] : state_->subscriptions) {
                Q_UNUSED(id);
                if (subscription.eventType == record.eventType) {
                    handlers.push_back(subscription.handler);
                }
            }
        }
        dispatch(handlers, record);
        ++replayed;
    }
    return replayed;
}

std::vector<EventRecord> EventBus::history() const {
    std::lock_guard lock(state_->mutex);
    return state_->history;
}
