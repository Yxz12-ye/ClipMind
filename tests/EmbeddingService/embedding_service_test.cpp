#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <QUrl>
#include <gtest/gtest.h>

#include "service/EmbeddingService.hpp"

namespace {

class FakeEmbeddingServer final : public QObject {
public:
    explicit FakeEmbeddingServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            auto* socket = server.nextPendingConnection();
            buffers.insert(socket, QByteArray());
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { readRequest(socket); });
            connect(socket, &QTcpSocket::disconnected, this,
                    [this, socket] { buffers.remove(socket); });
        });
    }

    bool start() { return server.listen(QHostAddress::LocalHost); }
    quint16 port() const { return server.serverPort(); }

    QByteArray requestBody;
    QByteArray requestContentType;
    QByteArray requestMethod;
    QString requestPath;
    int responseStatus = 200;
    QByteArray responseBody =
        R"({"data":[{"embedding":[0.25,-0.5],"index":0}],"model":"fake-model"})";
    int responseDelayMs = 0;

private:
    QTcpServer server;
    QHash<QTcpSocket*, QByteArray> buffers;

    void readRequest(QTcpSocket* socket) {
        QByteArray& buffer = buffers[socket];
        buffer += socket->readAll();
        const int headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }

        const QByteArray headers = buffer.left(headerEnd);
        const QList<QByteArray> lines = headers.split('\n');
        int contentLength = 0;
        if (!lines.isEmpty()) {
            const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
            if (requestLine.size() >= 2) {
                requestMethod = requestLine.first();
                requestPath =
                    QUrl(QStringLiteral("http://localhost") + QString::fromUtf8(requestLine.at(1)))
                        .path();
            }
        }

        for (const QByteArray& line : lines) {
            if (line.toLower().startsWith("content-length:")) {
                contentLength = line.mid(sizeof("content-length:") - 1).trimmed().toInt();
            } else if (line.toLower().startsWith("content-type:")) {
                requestContentType = line.mid(sizeof("content-type:") - 1).trimmed();
            }
        }

        const int bodyStart = headerEnd + 4;
        if (buffer.size() < bodyStart + contentLength) {
            return;
        }

        requestBody = buffer.mid(bodyStart, contentLength);
        buffer.clear();
        QTimer::singleShot(responseDelayMs, socket, [this, socket] { sendResponse(socket); });
    }

    void sendResponse(QTcpSocket* socket) {
        if (socket->state() != QAbstractSocket::ConnectedState) {
            return;
        }

        const QByteArray reason = responseStatus == 200 ? "OK" : "Error";
        const QByteArray response = "HTTP/1.1 " + QByteArray::number(responseStatus) + " " +
                                    reason + "\r\nContent-Type: application/json\r\n" +
                                    "Content-Length: " + QByteArray::number(responseBody.size()) +
                                    "\r\nConnection: close\r\n\r\n" + responseBody;
        socket->write(response);
        socket->disconnectFromHost();
    }
};

EmbeddingConfig configFor(const FakeEmbeddingServer& server, EmbeddingUrlMode mode) {
    EmbeddingConfig config;
    config.url = QStringLiteral("http://127.0.0.1:%1/v1").arg(server.port());
    config.model = QStringLiteral("fake-model");
    config.urlMode = mode;
    if (mode == EmbeddingUrlMode::FullEndpoint) {
        config.url += QStringLiteral("/embeddings");
    }
    return config;
}

bool waitForCompletion(QSignalSpy* successSpy, QSignalSpy* failureSpy, int timeoutMs = 1000) {
    QElapsedTimer timer;
    timer.start();
    while (successSpy->count() + failureSpy->count() == 0 && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QTest::qWait(1);
    }
    return successSpy->count() + failureSpy->count() == 1;
}

}  // namespace

TEST(EmbeddingServiceTest, PostsToFullEndpointAndParsesVector) {
    FakeEmbeddingServer server;
    ASSERT_TRUE(server.start());
    EmbeddingService service;
    QSignalSpy successSpy(&service, &EmbeddingService::embeddingSucceeded);
    QSignalSpy failureSpy(&service, &EmbeddingService::embeddingFailed);

    const quint64 requestId = service.embedText(QStringLiteral("hello"),
                                                configFor(server, EmbeddingUrlMode::FullEndpoint));
    ASSERT_TRUE(waitForCompletion(&successSpy, &failureSpy));

    ASSERT_EQ(successSpy.count(), 1);
    ASSERT_EQ(failureSpy.count(), 0);
    EXPECT_EQ(successSpy.at(0).at(0).toULongLong(), requestId);
    const auto result = qvariant_cast<EmbeddingResult>(successSpy.at(0).at(1));
    EXPECT_EQ(result.embedding, QVector<float>({0.25F, -0.5F}));
    EXPECT_EQ(result.model, QStringLiteral("fake-model"));
    EXPECT_EQ(server.requestMethod, QByteArray("POST"));
    EXPECT_EQ(server.requestPath, QStringLiteral("/v1/embeddings"));
    EXPECT_EQ(server.requestContentType, QByteArray("application/json"));

    const auto payload = QJsonDocument::fromJson(server.requestBody).object();
    EXPECT_EQ(payload.value(QStringLiteral("input")).toString(), QStringLiteral("hello"));
    EXPECT_EQ(payload.value(QStringLiteral("model")).toString(), QStringLiteral("fake-model"));
}

TEST(EmbeddingServiceTest, AppendsEmbeddingsToBaseUrl) {
    FakeEmbeddingServer server;
    ASSERT_TRUE(server.start());
    EmbeddingService service;
    QSignalSpy successSpy(&service, &EmbeddingService::embeddingSucceeded);
    QSignalSpy failureSpy(&service, &EmbeddingService::embeddingFailed);

    EmbeddingConfig config = configFor(server, EmbeddingUrlMode::BaseUrl);
    config.url += QLatin1Char('/');
    service.embedText(QStringLiteral("base-url"), config);
    ASSERT_TRUE(waitForCompletion(&successSpy, &failureSpy));

    ASSERT_EQ(successSpy.count(), 1);
    EXPECT_EQ(server.requestPath, QStringLiteral("/v1/embeddings"));
}

TEST(EmbeddingServiceTest, RejectsInvalidResponses) {
    const QVector<QByteArray> responses = {
        QByteArray("not-json"),
        QByteArray(R"({"data":[]})"),
        QByteArray(R"({"data":[{}]})"),
        QByteArray(R"({"data":[{"embedding":[]}]})"),
        QByteArray(R"({"data":[{"embedding":["bad"]}]})"),
    };

    for (const QByteArray& response : responses) {
        FakeEmbeddingServer server;
        ASSERT_TRUE(server.start());
        server.responseBody = response;
        EmbeddingService service;
        QSignalSpy successSpy(&service, &EmbeddingService::embeddingSucceeded);
        QSignalSpy failureSpy(&service, &EmbeddingService::embeddingFailed);

        service.embedText(QStringLiteral("invalid-response"),
                          configFor(server, EmbeddingUrlMode::FullEndpoint));
        ASSERT_TRUE(waitForCompletion(&successSpy, &failureSpy));

        EXPECT_EQ(successSpy.count(), 0);
        ASSERT_EQ(failureSpy.count(), 1);
        const auto error = qvariant_cast<EmbeddingError>(failureSpy.at(0).at(1));
        EXPECT_EQ(error.type, EmbeddingErrorType::InvalidResponse);
    }
}

TEST(EmbeddingServiceTest, ReportsHttpErrorMessage) {
    FakeEmbeddingServer server;
    ASSERT_TRUE(server.start());
    server.responseStatus = 401;
    server.responseBody = QByteArray(R"({"error":{"message":"unauthorized"}})");
    EmbeddingService service;
    QSignalSpy successSpy(&service, &EmbeddingService::embeddingSucceeded);
    QSignalSpy failureSpy(&service, &EmbeddingService::embeddingFailed);

    service.embedText(QStringLiteral("http-error"),
                      configFor(server, EmbeddingUrlMode::FullEndpoint));
    ASSERT_TRUE(waitForCompletion(&successSpy, &failureSpy));

    ASSERT_EQ(failureSpy.count(), 1);
    const auto error = qvariant_cast<EmbeddingError>(failureSpy.at(0).at(1));
    EXPECT_EQ(error.type, EmbeddingErrorType::Http);
    EXPECT_EQ(error.httpStatus, 401);
    EXPECT_EQ(error.message, QStringLiteral("unauthorized"));
}

TEST(EmbeddingServiceTest, TimesOutAndCanBeCancelled) {
    FakeEmbeddingServer timeoutServer;
    ASSERT_TRUE(timeoutServer.start());
    timeoutServer.responseDelayMs = 200;
    EmbeddingService timeoutService(nullptr, 30);
    QSignalSpy timeoutSuccess(&timeoutService, &EmbeddingService::embeddingSucceeded);
    QSignalSpy timeoutFailure(&timeoutService, &EmbeddingService::embeddingFailed);

    timeoutService.embedText(QStringLiteral("timeout"),
                             configFor(timeoutServer, EmbeddingUrlMode::FullEndpoint));
    ASSERT_TRUE(waitForCompletion(&timeoutSuccess, &timeoutFailure, 500));
    ASSERT_EQ(timeoutFailure.count(), 1);
    EXPECT_EQ(qvariant_cast<EmbeddingError>(timeoutFailure.at(0).at(1)).type,
              EmbeddingErrorType::Timeout);

    FakeEmbeddingServer cancelServer;
    ASSERT_TRUE(cancelServer.start());
    cancelServer.responseDelayMs = 200;
    EmbeddingService cancelService(nullptr, 1000);
    QSignalSpy cancelSuccess(&cancelService, &EmbeddingService::embeddingSucceeded);
    QSignalSpy cancelFailure(&cancelService, &EmbeddingService::embeddingFailed);

    const quint64 requestId = cancelService.embedText(
        QStringLiteral("cancel"), configFor(cancelServer, EmbeddingUrlMode::FullEndpoint));
    cancelService.cancelRequest(requestId);
    ASSERT_TRUE(waitForCompletion(&cancelSuccess, &cancelFailure, 500));
    ASSERT_EQ(cancelFailure.count(), 1);
    EXPECT_EQ(qvariant_cast<EmbeddingError>(cancelFailure.at(0).at(1)).type,
              EmbeddingErrorType::Cancelled);
}

TEST(EmbeddingServiceTest, ReportsInvalidConfigurationAndNetworkFailure) {
    EmbeddingService service(nullptr, 500);
    QSignalSpy successSpy(&service, &EmbeddingService::embeddingSucceeded);
    QSignalSpy failureSpy(&service, &EmbeddingService::embeddingFailed);

    EmbeddingConfig invalidConfig;
    invalidConfig.model = QStringLiteral("fake-model");
    service.embedText(QStringLiteral("invalid-config"), invalidConfig);
    ASSERT_TRUE(waitForCompletion(&successSpy, &failureSpy));
    ASSERT_EQ(failureSpy.count(), 1);
    EXPECT_EQ(qvariant_cast<EmbeddingError>(failureSpy.at(0).at(1)).type,
              EmbeddingErrorType::InvalidConfiguration);

    QTcpServer portProbe;
    ASSERT_TRUE(portProbe.listen(QHostAddress::LocalHost));
    const quint16 unusedPort = portProbe.serverPort();
    portProbe.close();

    successSpy.clear();
    failureSpy.clear();
    EmbeddingConfig networkConfig;
    networkConfig.url = QStringLiteral("http://127.0.0.1:%1/v1/embeddings").arg(unusedPort);
    networkConfig.model = QStringLiteral("fake-model");
    service.embedText(QStringLiteral("network-error"), networkConfig);
    ASSERT_TRUE(waitForCompletion(&successSpy, &failureSpy));
    ASSERT_EQ(failureSpy.count(), 1);
    EXPECT_EQ(qvariant_cast<EmbeddingError>(failureSpy.at(0).at(1)).type,
              EmbeddingErrorType::Network);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    qRegisterMetaType<EmbeddingResult>();
    qRegisterMetaType<EmbeddingError>();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
