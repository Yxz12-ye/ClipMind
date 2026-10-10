#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QUrl;

enum class EmbeddingUrlMode {
    FullEndpoint,
    BaseUrl,
};

struct EmbeddingConfig {
    QString url;
    QString model;
    EmbeddingUrlMode urlMode = EmbeddingUrlMode::FullEndpoint;

    bool operator==(const EmbeddingConfig&) const = default;
};

enum class EmbeddingErrorType {
    InvalidConfiguration,
    Network,
    Timeout,
    Http,
    InvalidResponse,
    Cancelled,
};

struct EmbeddingResult {
    QVector<float> embedding;
    QString model;
};

struct EmbeddingError {
    EmbeddingErrorType type = EmbeddingErrorType::Network;
    QString message;
    int httpStatus = 0;
};

Q_DECLARE_METATYPE(EmbeddingResult)
Q_DECLARE_METATYPE(EmbeddingError)

class EmbeddingService : public QObject {
    Q_OBJECT

public:
    explicit EmbeddingService(QObject* parent = nullptr, int requestTimeoutMs = 30000);
    ~EmbeddingService() override = default;

    void setConfig(const EmbeddingConfig& config);
    EmbeddingConfig config() const;
    // Each request captures its configuration; later changes do not affect pending requests.
    quint64 embedText(const QString& text);
    quint64 embedText(const QString& text, const EmbeddingConfig& config);
    void cancelRequest(quint64 requestId);

signals:
    void embeddingSucceeded(quint64 requestId, const EmbeddingResult& result);
    void embeddingFailed(quint64 requestId, const EmbeddingError& error);

private:
    struct PendingRequest {
        QNetworkReply* reply = nullptr;
        QTimer* timer = nullptr;
        QString requestedModel;
        QElapsedTimer elapsed;  // 请求发出到结束的耗时, 只用于日志
        bool timedOut = false;
        bool cancelled = false;
    };

    QNetworkAccessManager* networkManager;
    QHash<quint64, PendingRequest> pendingRequests;
    quint64 nextRequestId = 1;
    int requestTimeoutMs;
    EmbeddingConfig currentConfig;

    static bool resolveEndpoint(const QString& url, EmbeddingUrlMode urlMode, QUrl* endpoint,
                                QString* error);
    void finishRequest(quint64 requestId);
    void failLater(quint64 requestId, EmbeddingErrorType type, const QString& message,
                   int httpStatus = 0);
};
