#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <functional>
#include <gtest/gtest.h>
#include <memory>

#include "controller/UIController.hpp"

namespace {

class FakeCopyListener final : public AbstractCopyEventListener {
public:
    void copy(const QString& text) {
        lastText = text;
        emit clipboardChanged();
    }
    void getClipboardText() override {}

private:
    bool registerListenService() override { return true; }
};

class FakeEmbeddingServer final : public QObject {
public:
    FakeEmbeddingServer() {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            auto* socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { readRequest(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                buffers.remove(socket);
                socket->deleteLater();
            });
        });
    }
    bool start() { return server.listen(QHostAddress::LocalHost); }
    EmbeddingConfig config() const {
        return {QStringLiteral("http://127.0.0.1:%1/embeddings").arg(server.serverPort()),
                QStringLiteral("model-a"), EmbeddingUrlMode::FullEndpoint};
    }

    QVector<QString> inputs;
    QByteArray body = R"({"data":[{"embedding":[1,0]}],"model":"model-a"})";
    int status = 200;
    int delayMs = 20;

private:
    QTcpServer server;
    QHash<QTcpSocket*, QByteArray> buffers;

    void readRequest(QTcpSocket* socket) {
        QByteArray& buffer = buffers[socket];
        buffer += socket->readAll();
        const auto headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }
        int contentLength = 0;
        for (const auto& line : buffer.left(headerEnd).split('\n')) {
            if (line.toLower().startsWith("content-length:")) {
                contentLength = line.mid(sizeof("content-length:") - 1).trimmed().toInt();
            }
        }
        const auto bodyStart = headerEnd + 4;
        if (buffer.size() < bodyStart + contentLength) {
            return;
        }
        inputs.push_back(QJsonDocument::fromJson(buffer.mid(bodyStart, contentLength))
                             .object()
                             .value(QStringLiteral("input"))
                             .toString());
        buffer.clear();
        const QByteArray response =
            "HTTP/1.1 " + QByteArray::number(status) +
            " Response\r\nContent-Type: application/json\r\nContent-Length: " +
            QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        QTimer::singleShot(delayMs, socket, [socket, response] {
            if (socket->state() == QAbstractSocket::ConnectedState) {
                socket->write(response);
                socket->disconnectFromHost();
            }
        });
    }
};

bool waitUntil(const std::function<bool()>& predicate) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QTest::qWait(1);
    }
    return predicate();
}

QByteArray hashFor(const QString& content) {
    return ContentListItemData{Tag{}, content}.hash;
}

class UIControllerEmbeddingTest : public ::testing::Test {
protected:
    QTemporaryDir directory;
    FakeCopyListener listener;
    EmbeddingService embedding;
    FakeEmbeddingServer server;
    std::unique_ptr<SQLService> sql;
    std::unique_ptr<UIController> controller;

    void SetUp() override {
        ASSERT_TRUE(directory.isValid());
        ASSERT_TRUE(server.start());
        sql = std::make_unique<SQLService>(QDir(directory.filePath(QStringLiteral("db"))));
        ASSERT_FALSE(sql->getTags().isEmpty());
        embedding.setConfig(server.config());
        controller = std::make_unique<UIController>(&listener, &embedding, sql.get());
    }

    void saveVector(const QString& text, const QVector<float>& vector,
                    const QString& tagName = QStringLiteral("TEXT"),
                    const QString& model = QStringLiteral("model-a")) {
        const ContentListItemData item{Tag{tagName, {}, SearchMode::None}, text};
        ASSERT_TRUE(sql->save(item).isEmpty());
        ASSERT_TRUE(sql->saveEmbedding(item.hash, model, vector).isEmpty());
    }
};

TEST_F(UIControllerEmbeddingTest, SavesTextImmediatelyAndEmbedsOnceForDuplicateCopies) {
    const QString text = QStringLiteral("new clipboard text");
    QSignalSpy success(&embedding, &EmbeddingService::embeddingSucceeded);
    listener.copy(text);
    listener.copy(text);
    ASSERT_EQ(sql->get().size(), 1);
    EXPECT_TRUE(sql->getEmbedding(hashFor(text), QStringLiteral("model-a")).isEmpty());
    ASSERT_TRUE(waitUntil([&] { return success.count() == 1; }));
    EXPECT_EQ(server.inputs.size(), 1);
    EXPECT_EQ(server.inputs.front(), text);
    EXPECT_EQ(sql->getEmbedding(hashFor(text), QStringLiteral("model-a")),
              (QVector<float>{1.0f, 0.0f}));
    listener.copy(text);
    QTest::qWait(50);
    EXPECT_EQ(server.inputs.size(), 1);
    // Settings-page test requests must not be associated with clipboard content.
    embedding.embedText(QStringLiteral("endpoint test"), server.config());
    ASSERT_TRUE(waitUntil([&] { return success.count() == 2; }));
    EXPECT_EQ(sql->get().size(), 1);
    EXPECT_TRUE(
        sql->getEmbedding(hashFor(QStringLiteral("endpoint test")), QStringLiteral("model-a"))
            .isEmpty());
}

TEST_F(UIControllerEmbeddingTest, OutOfOrderResponsesUseOriginalHashAndConfiguration) {
    QSignalSpy success(&embedding, &EmbeddingService::embeddingSucceeded);
    server.delayMs = 150;
    listener.copy(QStringLiteral("first"));
    ASSERT_TRUE(waitUntil([&] { return server.inputs.size() == 1; }));
    auto config = server.config();
    config.model = QStringLiteral("model-b");
    embedding.setConfig(config);
    server.body = R"({"data":[{"embedding":[0,1]}],"model":"model-b"})";
    server.delayMs = 1;
    listener.copy(QStringLiteral("second"));
    ASSERT_TRUE(waitUntil([&] { return success.count() == 1; }));
    EXPECT_TRUE(
        sql->getEmbedding(hashFor(QStringLiteral("first")), QStringLiteral("model-a")).isEmpty());
    EXPECT_EQ(sql->getEmbedding(hashFor(QStringLiteral("second")), QStringLiteral("model-b")),
              (QVector<float>{0.0f, 1.0f}));
    ASSERT_TRUE(waitUntil([&] { return success.count() == 2; }));
    EXPECT_EQ(sql->getEmbedding(hashFor(QStringLiteral("first")), QStringLiteral("model-a")),
              (QVector<float>{1.0f, 0.0f}));
    EXPECT_TRUE(
        sql->getEmbedding(hashFor(QStringLiteral("first")), QStringLiteral("model-b")).isEmpty());
}

TEST_F(UIControllerEmbeddingTest, FailedRequestKeepsContentAndCanRetryOnNextCopy) {
    QSignalSpy failure(&embedding, &EmbeddingService::embeddingFailed);
    QSignalSpy success(&embedding, &EmbeddingService::embeddingSucceeded);
    server.status = 503;
    server.body = R"({"error":{"message":"unavailable"}})";
    const QString text = QStringLiteral("retry text");
    listener.copy(text);
    ASSERT_TRUE(waitUntil([&] { return failure.count() == 1; }));
    ASSERT_EQ(sql->get().size(), 1);
    EXPECT_TRUE(sql->getEmbedding(hashFor(text), QStringLiteral("model-a")).isEmpty());
    server.status = 200;
    server.body = R"({"data":[{"embedding":[1,0]}]})";
    listener.copy(text);
    ASSERT_TRUE(waitUntil([&] { return success.count() == 1; }));
    EXPECT_EQ(server.inputs.size(), 2);
    EXPECT_FALSE(sql->getEmbedding(hashFor(text), QStringLiteral("model-a")).isEmpty());
}

TEST_F(UIControllerEmbeddingTest, DeletionCancelsRequestAndDropsLateResultAfterRecopy) {
    QSignalSpy failure(&embedding, &EmbeddingService::embeddingFailed);
    QSignalSpy success(&embedding, &EmbeddingService::embeddingSucceeded);
    server.delayMs = 150;
    const QString text = QStringLiteral("deleted text");
    listener.copy(text);
    ASSERT_TRUE(waitUntil([&] { return server.inputs.size() == 1; }));
    controller->deleteItem(hashFor(text));
    ASSERT_TRUE(waitUntil([&] { return failure.count() == 1; }));
    EXPECT_EQ(qvariant_cast<EmbeddingError>(failure.at(0).at(1)).type,
              EmbeddingErrorType::Cancelled);
    EXPECT_TRUE(sql->get().isEmpty());
    const quint64 oldRequestId = failure.at(0).at(0).toULongLong();
    server.body = R"({"data":[{"embedding":[0,1]}]})";
    server.delayMs = 1;
    listener.copy(text);
    embedding.embeddingSucceeded(oldRequestId,
                                 EmbeddingResult{{1.0f, 0.0f}, QStringLiteral("model-a")});
    EXPECT_TRUE(sql->getEmbedding(hashFor(text), QStringLiteral("model-a")).isEmpty());
    ASSERT_TRUE(waitUntil([&] { return success.count() == 2; }));
    EXPECT_EQ(sql->getEmbedding(hashFor(text), QStringLiteral("model-a")),
              (QVector<float>{0.0f, 1.0f}));
}

TEST_F(UIControllerEmbeddingTest, SkipsBlankTextAndUnconfiguredEndpoint) {
    embedding.setConfig({});
    listener.copy(QStringLiteral("  \n"));
    EXPECT_TRUE(sql->get().isEmpty());
    listener.copy(QStringLiteral("offline text"));
    ASSERT_EQ(sql->get().size(), 1);
    QTest::qWait(50);
    EXPECT_TRUE(server.inputs.isEmpty());
}

TEST_F(UIControllerEmbeddingTest, StoresUnnamedServerDefaultWithEndpointKey) {
    auto config = server.config();
    config.model.clear();
    embedding.setConfig(config);
    server.body = R"({"data":[{"embedding":[1,0]}]})";
    QSignalSpy success(&embedding, &EmbeddingService::embeddingSucceeded);
    const QString text = QStringLiteral("default model text");
    listener.copy(text);
    ASSERT_TRUE(waitUntil([&] { return success.count() == 1; }));
    EXPECT_EQ(sql->getEmbedding(hashFor(text), QStringLiteral("default@endpoint:") + config.url),
              (QVector<float>{1.0f, 0.0f}));
}

TEST_F(UIControllerEmbeddingTest, ControllerDestructionCancelsOnlyItsOwnRequests) {
    server.delayMs = 150;
    QSignalSpy failure(&embedding, &EmbeddingService::embeddingFailed);
    QSignalSpy success(&embedding, &EmbeddingService::embeddingSucceeded);
    listener.copy(QStringLiteral("pending clipboard text"));
    const quint64 testId = embedding.embedText(QStringLiteral("endpoint test"), server.config());
    controller.reset();
    ASSERT_TRUE(waitUntil([&] { return failure.count() == 1 && success.count() == 1; }));
    EXPECT_EQ(success.at(0).at(0).toULongLong(), testId);
    EXPECT_TRUE(sql->getEmbedding(hashFor(QStringLiteral("pending clipboard text")),
                                  QStringLiteral("model-a"))
                    .isEmpty());
}

TEST_F(UIControllerEmbeddingTest, SemanticPrefixRanksVectorsAndReusesQueryForTagChanges) {
    saveVector(QStringLiteral("nearest"), {1.0f, 0.0f});
    saveVector(QStringLiteral("work result"), {0.8f, 0.6f}, QStringLiteral("WORK"));
    saveVector(QStringLiteral("farthest"), {0.0f, 1.0f});
    saveVector(QStringLiteral("other model"), {1.0f, 0.0f}, QStringLiteral("TEXT"),
               QStringLiteral("model-b"));
    QVector<ContentListItemData> view;
    QObject observer;
    QObject::connect(controller.get(), &UIController::updateUI, &observer,
                     [&](const QVector<ContentListItemData>& data) { view = data; });
    QSignalSpy success(&embedding, &EmbeddingService::embeddingSucceeded);
    controller->requireSearch(QStringLiteral("# query meaning"));
    EXPECT_TRUE(view.isEmpty());
    ASSERT_TRUE(waitUntil([&] { return success.count() == 1; }));
    ASSERT_EQ(server.inputs.size(), 1);
    EXPECT_EQ(server.inputs[0], QStringLiteral("query meaning"));
    ASSERT_EQ(view.size(), 3);
    EXPECT_EQ(view[0].content, QStringLiteral("nearest"));
    EXPECT_EQ(view[1].content, QStringLiteral("work result"));
    EXPECT_EQ(view[2].content, QStringLiteral("farthest"));
    EXPECT_EQ(sql->get().size(), 4);  // Query text is never saved as clipboard content.
    controller->requireTagFilter(QStringLiteral("WORK"));
    ASSERT_EQ(view.size(), 1);
    EXPECT_EQ(view[0].content, QStringLiteral("work result"));
    controller->requireTagFilter(QString());
    ASSERT_EQ(view.size(), 3);
    QTest::qWait(30);
    EXPECT_EQ(server.inputs.size(), 1);
}

TEST_F(UIControllerEmbeddingTest, SwitchingToRegexCancelsSemanticSearchAndIgnoresLateResult) {
    saveVector(QStringLiteral("ordinary match"), {1.0f, 0.0f});
    saveVector(QStringLiteral("unrelated"), {0.0f, 1.0f});
    QVector<ContentListItemData> view;
    QObject observer;
    QObject::connect(controller.get(), &UIController::updateUI, &observer,
                     [&](const QVector<ContentListItemData>& data) { view = data; });
    QSignalSpy failure(&embedding, &EmbeddingService::embeddingFailed);
    server.delayMs = 150;
    controller->requireSearch(QStringLiteral("# old query"));
    ASSERT_TRUE(waitUntil([&] { return server.inputs.size() == 1; }));
    controller->requireSearch(QStringLiteral("ordinary"));
    ASSERT_EQ(view.size(), 1);
    EXPECT_EQ(view[0].content, QStringLiteral("ordinary match"));
    ASSERT_TRUE(waitUntil([&] { return failure.count() == 1; }));
    const quint64 oldId = failure.at(0).at(0).toULongLong();
    embedding.embeddingSucceeded(oldId, EmbeddingResult{{0.0f, 1.0f}, QStringLiteral("model-a")});
    ASSERT_EQ(view.size(), 1);
    EXPECT_EQ(view[0].content, QStringLiteral("ordinary match"));
}

TEST_F(UIControllerEmbeddingTest, ReplacingSemanticQueryUsesOnlyLatestVector) {
    saveVector(QStringLiteral("first direction"), {1.0f, 0.0f});
    saveVector(QStringLiteral("second direction"), {0.0f, 1.0f});
    QVector<ContentListItemData> view;
    QObject observer;
    QObject::connect(controller.get(), &UIController::updateUI, &observer,
                     [&](const QVector<ContentListItemData>& data) { view = data; });
    QSignalSpy failure(&embedding, &EmbeddingService::embeddingFailed);
    QSignalSpy success(&embedding, &EmbeddingService::embeddingSucceeded);
    server.delayMs = 150;
    controller->requireSearch(QStringLiteral("# first"));
    ASSERT_TRUE(waitUntil([&] { return server.inputs.size() == 1; }));
    server.body = R"({"data":[{"embedding":[0,1]}]})";
    server.delayMs = 1;
    controller->requireSearch(QStringLiteral("# second"));
    ASSERT_TRUE(waitUntil([&] { return failure.count() == 1 && success.count() == 1; }));
    ASSERT_EQ(view.size(), 2);
    EXPECT_EQ(view.front().content, QStringLiteral("second direction"));
    const quint64 oldId = failure.at(0).at(0).toULongLong();
    embedding.embeddingSucceeded(oldId, EmbeddingResult{{1.0f, 0.0f}, QStringLiteral("model-a")});
    EXPECT_EQ(view.front().content, QStringLiteral("second direction"));
    EXPECT_EQ(server.inputs.back(), QStringLiteral("second"));
}

TEST_F(UIControllerEmbeddingTest, OrdinaryQueriesUseRegexIncludingInsideTags) {
    saveVector(QStringLiteral("Project 123"), {1.0f}, QStringLiteral("WORK"));
    saveVector(QStringLiteral("Project notes"), {1.0f}, QStringLiteral("WORK"));
    saveVector(QStringLiteral("#hashtag [draft"), {1.0f});
    QVector<ContentListItemData> view;
    QObject observer;
    QObject::connect(controller.get(), &UIController::updateUI, &observer,
                     [&](const QVector<ContentListItemData>& data) { view = data; });
    controller->requireSearch(QStringLiteral("project"));
    EXPECT_EQ(view.size(), 2);
    controller->requireSearch(QStringLiteral("^project \\d+$"));
    ASSERT_EQ(view.size(), 1);
    EXPECT_EQ(view[0].content, QStringLiteral("Project 123"));
    controller->requireTagFilter(QStringLiteral("WORK"));
    ASSERT_EQ(view.size(), 1);
    EXPECT_EQ(view[0].content, QStringLiteral("Project 123"));
    controller->requireTagFilter(QString());
    controller->requireSearch(QStringLiteral("[draft"));
    ASSERT_EQ(view.size(), 1);
    controller->requireSearch(QStringLiteral("#hashtag"));
    ASSERT_EQ(view.size(), 1);
    EXPECT_EQ(view[0].content, QStringLiteral("#hashtag [draft"));
    controller->requireSearch(QStringLiteral("# "));
    EXPECT_EQ(view.size(), 3);
    controller->requireSearch(QString());
    EXPECT_EQ(view.size(), 3);
    QTest::qWait(30);
    EXPECT_TRUE(server.inputs.isEmpty());
}

TEST_F(UIControllerEmbeddingTest, SemanticFailureDoesNotRestoreUnrelatedHistory) {
    saveVector(QStringLiteral("history"), {1.0f, 0.0f});
    QVector<ContentListItemData> view;
    QObject observer;
    QObject::connect(controller.get(), &UIController::updateUI, &observer,
                     [&](const QVector<ContentListItemData>& data) { view = data; });
    controller->requireSearch(QString());
    ASSERT_EQ(view.size(), 1);
    server.status = 503;
    server.body = R"({"error":{"message":"unavailable"}})";
    QSignalSpy failure(&embedding, &EmbeddingService::embeddingFailed);
    controller->requireSearch(QStringLiteral("# meaning"));
    ASSERT_TRUE(waitUntil([&] { return failure.count() == 1; }));
    EXPECT_TRUE(view.isEmpty());
    embedding.setConfig({});
    controller->requireSearch(QStringLiteral("# another"));
    EXPECT_TRUE(view.isEmpty());
    EXPECT_EQ(sql->get().size(), 1);
}

TEST_F(UIControllerEmbeddingTest, NewEmbeddingRefreshesSemanticResultsWithoutEmbeddingQueryAgain) {
    saveVector(QStringLiteral("existing"), {1.0f, 0.0f});
    QVector<ContentListItemData> view;
    QObject observer;
    QObject::connect(controller.get(), &UIController::updateUI, &observer,
                     [&](const QVector<ContentListItemData>& data) { view = data; });
    QSignalSpy success(&embedding, &EmbeddingService::embeddingSucceeded);
    controller->requireSearch(QStringLiteral("# meaning"));
    ASSERT_TRUE(waitUntil([&] { return success.count() == 1; }));
    ASSERT_EQ(view.size(), 1);
    listener.copy(QStringLiteral("new content"));
    ASSERT_TRUE(waitUntil([&] { return success.count() == 2; }));
    EXPECT_EQ(view.size(), 2);
    ASSERT_EQ(server.inputs.size(), 2);
    EXPECT_EQ(server.inputs[0], QStringLiteral("meaning"));
    EXPECT_EQ(server.inputs[1], QStringLiteral("new content"));
}

TEST_F(UIControllerEmbeddingTest, UnnamedDefaultModelIsSharedByContentAndSemanticSearch) {
    auto config = server.config();
    config.model.clear();
    embedding.setConfig(config);
    server.body = R"({"data":[{"embedding":[1,0]}]})";
    QSignalSpy success(&embedding, &EmbeddingService::embeddingSucceeded);
    listener.copy(QStringLiteral("content"));
    ASSERT_TRUE(waitUntil([&] { return success.count() == 1; }));
    QVector<ContentListItemData> view;
    QObject observer;
    QObject::connect(controller.get(), &UIController::updateUI, &observer,
                     [&](const QVector<ContentListItemData>& data) { view = data; });
    controller->requireSearch(QStringLiteral("# meaning"));
    ASSERT_TRUE(waitUntil([&] { return success.count() == 2; }));
    ASSERT_EQ(view.size(), 1);
    EXPECT_EQ(view[0].content, QStringLiteral("content"));
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    qRegisterMetaType<EmbeddingResult>();
    qRegisterMetaType<EmbeddingError>();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
