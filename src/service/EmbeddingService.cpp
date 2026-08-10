#include "EmbeddingService.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

namespace {

QString responseErrorMessage(const QByteArray& body) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return QString();
    }

    const QJsonValue errorValue = document.object().value(QStringLiteral("error"));
    if (errorValue.isObject()) {
        return errorValue.toObject().value(QStringLiteral("message")).toString().trimmed();
    }

    if (errorValue.isString()) {
        return errorValue.toString().trimmed();
    }

    return QString();
}

}  // namespace

EmbeddingService::EmbeddingService(QObject* parent, int requestTimeoutMs)
    : QObject(parent),
      networkManager(new QNetworkAccessManager(this)),
      requestTimeoutMs(qMax(1, requestTimeoutMs)) {}

quint64 EmbeddingService::embedText(const QString& text, const EmbeddingConfig& config) {
    const quint64 requestId = nextRequestId++;

    if (text.trimmed().isEmpty()) {
        failLater(requestId, EmbeddingErrorType::InvalidConfiguration,
                  QStringLiteral("测试文本不能为空"));
        return requestId;
    }

    QUrl endpoint;
    QString configurationError;
    if (!resolveEndpoint(config, &endpoint, &configurationError)) {
        failLater(requestId, EmbeddingErrorType::InvalidConfiguration, configurationError);
        return requestId;
    }

    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");

    QJsonObject payload;
    payload.insert(QStringLiteral("input"), text);
    payload.insert(QStringLiteral("model"), config.model.trimmed());

    QNetworkReply* reply = networkManager->post(request, QJsonDocument(payload).toJson());
    auto* timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);

    PendingRequest pending;
    pending.reply = reply;
    pending.timer = timeoutTimer;
    pending.requestedModel = config.model.trimmed();
    pendingRequests.insert(requestId, pending);

    connect(timeoutTimer, &QTimer::timeout, this, [this, requestId] {
        auto request = pendingRequests.find(requestId);
        if (request == pendingRequests.end()) {
            return;
        }

        request->timedOut = true;
        request->reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, requestId] { finishRequest(requestId); });

    timeoutTimer->start(requestTimeoutMs);
    return requestId;
}

void EmbeddingService::cancelRequest(quint64 requestId) {
    auto request = pendingRequests.find(requestId);
    if (request == pendingRequests.end()) {
        return;
    }

    request->cancelled = true;
    request->reply->abort();
}

bool EmbeddingService::resolveEndpoint(const EmbeddingConfig& config, QUrl* endpoint,
                                       QString* error) {
    const QString urlText = config.url.trimmed();
    if (urlText.isEmpty()) {
        *error = QStringLiteral("请输入接口 URL");
        return false;
    }

    if (config.model.trimmed().isEmpty()) {
        *error = QStringLiteral("请输入模型名称");
        return false;
    }

    QUrl resolved(urlText);
    const QString scheme = resolved.scheme().toLower();
    if (!resolved.isValid() || resolved.isRelative() ||
        (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) ||
        resolved.host().isEmpty()) {
        *error = QStringLiteral("接口 URL 必须是有效的 HTTP 或 HTTPS 地址");
        return false;
    }

    if (resolved.hasFragment()) {
        *error = QStringLiteral("接口 URL 不能包含片段标识");
        return false;
    }

    if (config.urlMode == EmbeddingUrlMode::BaseUrl) {
        QString path = resolved.path();
        while (path.endsWith(QLatin1Char('/'))) {
            path.chop(1);
        }
        resolved.setPath(path + QStringLiteral("/embeddings"));
    }

    *endpoint = resolved;
    return true;
}

void EmbeddingService::finishRequest(quint64 requestId) {
    auto requestIterator = pendingRequests.find(requestId);
    if (requestIterator == pendingRequests.end()) {
        return;
    }

    const PendingRequest request = requestIterator.value();
    pendingRequests.erase(requestIterator);
    request.timer->stop();

    QNetworkReply* reply = request.reply;
    const QByteArray body = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (request.cancelled) {
        emit embeddingFailed(requestId, EmbeddingError{EmbeddingErrorType::Cancelled,
                                                       QStringLiteral("请求已取消"), httpStatus});
        reply->deleteLater();
        return;
    }

    if (request.timedOut) {
        emit embeddingFailed(
            requestId,
            EmbeddingError{EmbeddingErrorType::Timeout,
                           QStringLiteral("请求超时，请检查服务地址或稍后重试"), httpStatus});
        reply->deleteLater();
        return;
    }

    if (httpStatus != 0 && (httpStatus < 200 || httpStatus >= 300)) {
        QString message = responseErrorMessage(body);
        if (message.isEmpty()) {
            message = QString::fromUtf8(body).trimmed().left(500);
        }
        if (message.isEmpty()) {
            message = reply->errorString();
        }
        if (message.isEmpty()) {
            message = QStringLiteral("请求失败");
        }
        emit embeddingFailed(requestId,
                             EmbeddingError{EmbeddingErrorType::Http, message, httpStatus});
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString message = reply->errorString().trimmed();
        if (message.isEmpty()) {
            message = QStringLiteral("网络请求失败");
        }
        emit embeddingFailed(requestId,
                             EmbeddingError{EmbeddingErrorType::Network, message, httpStatus});
        reply->deleteLater();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit embeddingFailed(
            requestId, EmbeddingError{EmbeddingErrorType::InvalidResponse,
                                      QStringLiteral("服务返回了无效的 JSON 响应"), httpStatus});
        reply->deleteLater();
        return;
    }

    const QJsonObject response = document.object();
    const QJsonValue dataValue = response.value(QStringLiteral("data"));
    if (!dataValue.isArray() || dataValue.toArray().isEmpty() ||
        !dataValue.toArray().first().isObject()) {
        emit embeddingFailed(requestId,
                             EmbeddingError{EmbeddingErrorType::InvalidResponse,
                                            QStringLiteral("响应中缺少 data[0]"), httpStatus});
        reply->deleteLater();
        return;
    }

    const QJsonValue embeddingValue =
        dataValue.toArray().first().toObject().value(QStringLiteral("embedding"));
    if (!embeddingValue.isArray() || embeddingValue.toArray().isEmpty()) {
        emit embeddingFailed(
            requestId,
            EmbeddingError{EmbeddingErrorType::InvalidResponse,
                           QStringLiteral("响应中缺少有效的 embedding 向量"), httpStatus});
        reply->deleteLater();
        return;
    }

    QVector<float> embedding;
    const QJsonArray embeddingArray = embeddingValue.toArray();
    embedding.reserve(embeddingArray.size());
    for (const QJsonValue& value : embeddingArray) {
        if (!value.isDouble()) {
            emit embeddingFailed(
                requestId,
                EmbeddingError{EmbeddingErrorType::InvalidResponse,
                               QStringLiteral("embedding 向量包含非数字元素"), httpStatus});
            reply->deleteLater();
            return;
        }
        embedding.append(static_cast<float>(value.toDouble()));
    }

    QString responseModel = response.value(QStringLiteral("model")).toString().trimmed();
    if (responseModel.isEmpty()) {
        responseModel = request.requestedModel;
    }

    emit embeddingSucceeded(requestId, EmbeddingResult{embedding, responseModel});
    reply->deleteLater();
}

void EmbeddingService::failLater(quint64 requestId, EmbeddingErrorType type, const QString& message,
                                 int httpStatus) {
    QTimer::singleShot(0, this, [this, requestId, type, message, httpStatus] {
        emit embeddingFailed(requestId, EmbeddingError{type, message, httpStatus});
    });
}
