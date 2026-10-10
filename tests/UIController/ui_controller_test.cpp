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

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    qRegisterMetaType<EmbeddingResult>();
    qRegisterMetaType<EmbeddingError>();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
