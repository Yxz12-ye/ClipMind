#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPoint>
#include <QTemporaryDir>
#include <atomic>
#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include "service/EventBus.hpp"

namespace {

const QString kCopied = QStringLiteral("clipboard.copied");

static_assert(!std::is_copy_constructible_v<PublisherHandle>);
static_assert(!std::is_copy_constructible_v<SubscriptionToken>);

void writeJson(const QString& path, const QJsonObject& object) {
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    const auto bytes = QJsonDocument(object).toJson();
    ASSERT_EQ(file.write(bytes), bytes.size());
}

QJsonObject eventJson(const QString& type, const QString& publisher, const QDateTime& time) {
    return {{QStringLiteral("eventType"), type},
            {QStringLiteral("publisherId"), publisher},
            {QStringLiteral("timestamp"), time.toString(Qt::ISODateWithMs)},
            {QStringLiteral("parameters"), QJsonObject{}}};
}

class Subscriber {
public:
    void onEvent(const EventRecord& record) { received.push_back(record); }
    std::vector<EventRecord> received;
};

void registerCopied(EventBus& bus) {
    ASSERT_TRUE(bus.registerEventType(kCopied));
}

}  // namespace

TEST(EventBusTest, RegistersPublishersAndDispatchesParameters) {
    EventBus bus;
    ASSERT_TRUE(bus.registerEventType(kCopied));
    auto publisher = bus.registerPublisher(QStringLiteral("clipboard"));
    ASSERT_TRUE(publisher.has_value());
    Subscriber subscriber;
    auto token = bus.subscribe(
        kCopied, [&subscriber](const EventRecord& event) { subscriber.onEvent(event); });
    ASSERT_TRUE(token.has_value());
    ASSERT_TRUE(publisher->publish(kCopied, {{QStringLiteral("text"), QStringLiteral("hello")}}));
    ASSERT_EQ(subscriber.received.size(), 1U);
    EXPECT_EQ(subscriber.received.front().parameters.value(QStringLiteral("text")).toString(),
              QStringLiteral("hello"));
}

TEST(EventBusTest, TokenUnsubscribesAutomatically) {
    EventBus bus;
    registerCopied(bus);
    auto publisher = bus.registerPublisher(QStringLiteral("clipboard"));
    ASSERT_TRUE(publisher.has_value());
    int count = 0;
    {
        auto token = bus.subscribe(kCopied, [&count](const EventRecord&) { ++count; });
        ASSERT_TRUE(token.has_value());
        ASSERT_TRUE(publisher->publish(kCopied));
    }
    ASSERT_TRUE(publisher->publish(kCopied));
    EXPECT_EQ(count, 1);
}

TEST(EventBusTest, HandlerExceptionDoesNotStopOtherHandlers) {
    EventBus bus;
    registerCopied(bus);
    auto publisher = bus.registerPublisher(QStringLiteral("clipboard"));
    ASSERT_TRUE(publisher.has_value());
    int count = 0;
    auto throwingToken =
        bus.subscribe(kCopied, [](const EventRecord&) { throw std::runtime_error("boom"); });
    auto countingToken = bus.subscribe(kCopied, [&count](const EventRecord&) { ++count; });
    ASSERT_TRUE(throwingToken.has_value());
    ASSERT_TRUE(countingToken.has_value());
    ASSERT_TRUE(publisher->publish(kCopied));
    EXPECT_EQ(count, 1);
}

TEST(EventBusTest, SavesLoadsAndFiltersReplay) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString filePath = directory.filePath(QStringLiteral("events.json"));
    EventBus source;
    registerCopied(source);
    auto publisher = source.registerPublisher(QStringLiteral("clipboard"));
    ASSERT_TRUE(publisher.has_value());
    ASSERT_TRUE(publisher->publish(kCopied, {{QStringLiteral("value"), 1}}));
    ASSERT_TRUE(source.save(filePath));

    EventBus restored;
    registerCopied(restored);
    ASSERT_TRUE(restored.load(filePath));
    int count = 0;
    auto token = restored.subscribe(kCopied, [&count](const EventRecord&) { ++count; });
    ASSERT_TRUE(token.has_value());
    ReplayFilter filter;
    filter.eventType = kCopied;
    EXPECT_EQ(restored.replay(filter), 1U);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(restored.history().size(), 1U);
}

TEST(EventBusTest, RejectsInvalidAndDuplicateRegistrations) {
    EventTypeRegistry registry;
    EXPECT_FALSE(registry.registerType(QStringLiteral("  ")));
    EXPECT_TRUE(registry.registerType(kCopied));
    EXPECT_FALSE(registry.registerType(kCopied));
    EXPECT_TRUE(registry.contains(kCopied));
    EventBus bus;
    registerCopied(bus);
    EXPECT_FALSE(bus.registerPublisher(QString()));
    auto publisher = bus.registerPublisher(QStringLiteral("source"));
    ASSERT_TRUE(publisher);
    EXPECT_FALSE(bus.registerPublisher(QStringLiteral("source")));
    EXPECT_FALSE(bus.subscribe(QStringLiteral("unknown"), [](const EventRecord&) {}));
    EXPECT_FALSE(bus.subscribe(kCopied, {}));
    EXPECT_FALSE(publisher->publish(QStringLiteral("unknown")));
    EXPECT_FALSE(publisher->publish(kCopied, {{QStringLiteral(" "), 1}}));
    EXPECT_TRUE(bus.history().empty());
}

TEST(EventBusTest, HandlesMoveAndBusLifetime) {
    PublisherHandle surviving;
    SubscriptionToken survivingToken;
    {
        EventBus bus;
        registerCopied(bus);
        auto publisher = bus.registerPublisher(QStringLiteral("source"));
        ASSERT_TRUE(publisher);
        surviving = std::move(*publisher);
        EXPECT_FALSE(publisher->isValid());
        EXPECT_FALSE(publisher->publish(kCopied));
        auto token = bus.subscribe(kCopied, [](const EventRecord&) {});
        ASSERT_TRUE(token);
        survivingToken = std::move(*token);
        EXPECT_FALSE(token->isValid());
        EXPECT_TRUE(surviving.publish(kCopied));
    }
    EXPECT_FALSE(surviving.isValid());
    EXPECT_FALSE(surviving.publish(kCopied));
    EXPECT_FALSE(survivingToken.isValid());
    survivingToken.unsubscribe();
}

TEST(EventBusTest, DispatchOrderAndReentrantCancellation) {
    EventBus bus;
    registerCopied(bus);
    auto publisher = bus.registerPublisher(QStringLiteral("source"));
    ASSERT_TRUE(publisher);
    std::vector<int> order;
    std::optional<SubscriptionToken> first;
    first = bus.subscribe(kCopied, [&](const EventRecord&) {
        order.push_back(1);
        first->unsubscribe();
        EXPECT_EQ(bus.history().size(), 1U);
    });
    auto second = bus.subscribe(kCopied, [&](const EventRecord&) { order.push_back(2); });
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_TRUE(publisher->publish(kCopied));
    EXPECT_EQ(order, (std::vector<int>{1, 2}));
    second->unsubscribe();
    second->unsubscribe();
    EXPECT_TRUE(publisher->publish(kCopied));
    EXPECT_EQ(order.size(), 2U);
}

TEST(EventBusTest, MoveAssignmentCancelsPreviousSubscription) {
    EventBus bus;
    registerCopied(bus);
    auto publisher = bus.registerPublisher(QStringLiteral("source"));
    ASSERT_TRUE(publisher);
    int firstCount = 0;
    int secondCount = 0;
    auto first = bus.subscribe(kCopied, [&](const EventRecord&) { ++firstCount; });
    auto second = bus.subscribe(kCopied, [&](const EventRecord&) { ++secondCount; });
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    *first = std::move(*second);
    ASSERT_TRUE(publisher->publish(kCopied));
    EXPECT_EQ(firstCount, 0);
    EXPECT_EQ(secondCount, 1);
}

TEST(EventBusTest, FailedLoadKeepsHistoryAndFailedSaveKeepsFile) {
    EventBus bus;
    registerCopied(bus);
    auto publisher = bus.registerPublisher(QStringLiteral("source"));
    ASSERT_TRUE(publisher);
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("events.json"));
    ASSERT_TRUE(publisher->publish(kCopied));
    ASSERT_TRUE(bus.save(path));
    QFile original(path);
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    const auto bytes = original.readAll();
    original.close();
    ASSERT_TRUE(publisher->publish(
        kCopied, {{QStringLiteral("nested"), QVariantList{QVariant::fromValue(QPoint(1, 2))}}}));
    EXPECT_FALSE(bus.save(path));
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    EXPECT_EQ(original.readAll(), bytes);
    original.close();
    const auto time = QDateTime::currentDateTimeUtc();
    auto good = eventJson(kCopied, QStringLiteral("source"), time);
    auto wrongTimestamp = good;
    wrongTimestamp.insert(QStringLiteral("timestamp"), QStringLiteral("invalid"));
    auto wrongParameters = good;
    wrongParameters.insert(QStringLiteral("parameters"), QJsonArray{});
    auto wrongPublisher = good;
    wrongPublisher.insert(QStringLiteral("publisherId"), 42);
    const std::vector<QJsonObject> invalid{
        QJsonObject{},
        {{QStringLiteral("version"), 2}, {QStringLiteral("events"), QJsonArray{good}}},
        {{QStringLiteral("version"), 1},
         {QStringLiteral("events"),
          QJsonArray{good, eventJson(QStringLiteral("unknown"), QStringLiteral("source"), time)}}},
        {{QStringLiteral("version"), 1}, {QStringLiteral("events"), QJsonArray{QJsonObject{}}}}};
    for (const auto& record : {wrongTimestamp, wrongParameters, wrongPublisher}) {
        writeJson(path, {{QStringLiteral("version"), 1},
                         {QStringLiteral("events"), QJsonArray{good, record}}});
        EXPECT_FALSE(bus.load(path));
        EXPECT_EQ(bus.history().size(), 2U);
    }
    for (const auto& root : invalid) {
        writeJson(path, root);
        EXPECT_FALSE(bus.load(path));
        EXPECT_EQ(bus.history().size(), 2U);
    }
    EXPECT_FALSE(bus.load(directory.filePath(QStringLiteral("missing.json"))));
    EXPECT_EQ(bus.history().size(), 2U);
    QFile malformed(path);
    ASSERT_TRUE(malformed.open(QIODevice::WriteOnly | QIODevice::Truncate));
    ASSERT_EQ(malformed.write("{broken"), 7);
    malformed.close();
    EXPECT_FALSE(bus.load(path));
    EXPECT_EQ(bus.history().size(), 2U);
}

TEST(EventBusTest, NestedParametersRoundTripAndEmptyHistoryReplacesOldHistory) {
    EventBus bus;
    registerCopied(bus);
    auto publisher = bus.registerPublisher(QStringLiteral("source"));
    ASSERT_TRUE(publisher);
    const QVariantMap parameters{
        {QStringLiteral("nested"),
         QVariantMap{{QStringLiteral("items"), QVariantList{true, QStringLiteral("text"), 1.5}}}}};
    ASSERT_TRUE(publisher->publish(kCopied, parameters));
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("events.json"));
    ASSERT_TRUE(bus.save(path));
    ASSERT_TRUE(bus.load(path));
    ASSERT_EQ(bus.history().size(), 1U);
    EXPECT_EQ(bus.history().front().parameters, parameters);
    writeJson(path, {{QStringLiteral("version"), 1}, {QStringLiteral("events"), QJsonArray{}}});
    ASSERT_TRUE(bus.load(path));
    EXPECT_TRUE(bus.history().empty());
}

TEST(EventBusTest, ReplayFiltersInclusiveTimesAndPreservesMetadata) {
    EventBus bus;
    registerCopied(bus);
    ASSERT_TRUE(bus.registerEventType(QStringLiteral("other")));
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("events.json"));
    const auto start =
        QDateTime::fromString(QStringLiteral("2026-01-01T00:00:00.000Z"), Qt::ISODateWithMs);
    QJsonArray events{eventJson(kCopied, QStringLiteral("a"), start),
                      eventJson(kCopied, QStringLiteral("b"), start.addSecs(1)),
                      eventJson(QStringLiteral("other"), QStringLiteral("a"), start.addSecs(2)),
                      eventJson(kCopied, QStringLiteral("a"), start.addSecs(3))};
    writeJson(path, {{QStringLiteral("version"), 1}, {QStringLiteral("events"), events}});
    ASSERT_TRUE(bus.load(path));
    Subscriber subscriber;
    auto token =
        bus.subscribe(kCopied, [&](const EventRecord& record) { subscriber.onEvent(record); });
    ASSERT_TRUE(token);
    ReplayFilter filter;
    filter.eventType = kCopied;
    filter.publisherId = QStringLiteral("a");
    filter.from = start;
    filter.to = start.addSecs(3);
    EXPECT_EQ(bus.replay(filter), 2U);
    ASSERT_EQ(subscriber.received.size(), 2U);
    EXPECT_EQ(subscriber.received.front().timestamp, start);
    EXPECT_EQ(subscriber.received.back().timestamp, start.addSecs(3));
    EXPECT_EQ(bus.history().size(), 4U);
    filter.from = start.addSecs(4);
    EXPECT_EQ(bus.replay(filter), 0U);
    filter = {};
    filter.eventType = QStringLiteral("unknown");
    EXPECT_EQ(bus.replay(filter), 0U);
    EXPECT_EQ(bus.replay(), 4U);
}

TEST(EventBusTest, ConcurrentRegistrationsSubscriptionsAndPublications) {
    EventBus bus;
    registerCopied(bus);
    std::atomic<int> received{0};
    std::atomic<int> failures{0};
    auto permanent = bus.subscribe(kCopied, [&](const EventRecord&) { ++received; });
    ASSERT_TRUE(permanent);
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&, i] {
            if (!bus.registerEventType(QStringLiteral("type%1").arg(i))) {
                ++failures;
            }
            auto publisher = bus.registerPublisher(QStringLiteral("source%1").arg(i));
            if (!publisher) {
                ++failures;
                return;
            }
            for (int j = 0; j < 100; ++j) {
                auto temporary = bus.subscribe(kCopied, [](const EventRecord&) {});
                if (!temporary || !publisher->publish(kCopied, {{QStringLiteral("index"), j}})) {
                    ++failures;
                }
                if (temporary)
                    temporary->unsubscribe();
            }
        });
    }
    for (auto& worker : workers)
        worker.join();
    EXPECT_EQ(failures.load(), 0);
    EXPECT_EQ(received.load(), 400);
    EXPECT_EQ(bus.history().size(), 400U);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
