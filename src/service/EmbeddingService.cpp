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

#include <string_view>

#include "LogService.hpp"

namespace {

// 日志模块名, 与 LogService 的用法保持一致
constexpr std::string_view kLogModule = "EmbeddingService";

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

// 请求里没带模型时, 服务端可能直接报错; 这类失败在日志里补一句提醒
const QString kEmptyModelHint =
    QStringLiteral(" (请求未携带模型标识, 若服务端要求指定模型, 请在设置里填写模型)");

}  // namespace

EmbeddingService::EmbeddingService(QObject* parent, int requestTimeoutMs)
    : QObject(parent),
      networkManager(new QNetworkAccessManager(this)),
      requestTimeoutMs(qMax(1, requestTimeoutMs)) {}

quint64 EmbeddingService::embedText(const QString& text, const EmbeddingConfig& config) {
    const quint64 requestId = nextRequestId++;

    if (text.trimmed().isEmpty()) {
        LogService::warn(kLogModule, "embedding request #{} rejected: empty test text", requestId);
        failLater(requestId, EmbeddingErrorType::InvalidConfiguration,
                  QStringLiteral("测试文本不能为空"));
        return requestId;
    }

    QUrl endpoint;
    QString configurationError;
    if (!resolveEndpoint(config.url, config.urlMode, &endpoint, &configurationError)) {
        LogService::warn(kLogModule, "embedding request #{} rejected: {} (url: '{}', model: '{}')",
                         requestId, configurationError, config.url, config.model);
        failLater(requestId, EmbeddingErrorType::InvalidConfiguration, configurationError);
        return requestId;
    }

    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");

    // 模型标识留空时原样发给服务端(空串), 由服务端套用默认模型; 这里不做本地拦截
    const QString requestedModel = config.model.trimmed();
    QJsonObject payload;
    payload.insert(QStringLiteral("input"), text);
    payload.insert(QStringLiteral("model"), requestedModel);

    LogService::debug(kLogModule,
                      "embedding request #{} -> {} (model: '{}', url mode: {}, text length: {})",
                      requestId, endpoint.toString(QUrl::FullyEncoded), requestedModel,
                      config.urlMode == EmbeddingUrlMode::BaseUrl ? "base-url" : "full-endpoint",
                      text.size());
    if (requestedModel.isEmpty()) {
        LogService::debug(kLogModule,
                          "embedding request #{} carries no model identifier, the server default "
                          "will be used",
                          requestId);
    }

    QNetworkReply* reply = networkManager->post(request, QJsonDocument(payload).toJson());
    auto* timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);

    PendingRequest pending;
    pending.reply = reply;
    pending.timer = timeoutTimer;
    pending.requestedModel = requestedModel;
    pending.elapsed.start();
    pendingRequests.insert(requestId, pending);

    connect(timeoutTimer, &QTimer::timeout, this, [this, requestId] {
        auto request = pendingRequests.find(requestId);
        if (request == pendingRequests.end()) {
            return;
        }

        LogService::warn(kLogModule, "embedding request #{} timed out after {} ms, aborting",
                         requestId, requestTimeoutMs);
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
        LogService::debug(kLogModule, "cancel ignored, embedding request #{} is not pending",
                          requestId);
        return;
    }

    LogService::debug(kLogModule, "embedding request #{} cancelled", requestId);
    request->cancelled = true;
    request->reply->abort();
}

bool EmbeddingService::resolveEndpoint(const QString& url, EmbeddingUrlMode urlMode, QUrl* endpoint,
                                       QString* error) {
    const QString urlText = url.trimmed();
    if (urlText.isEmpty()) {
        *error = QStringLiteral("请输入接口 URL");
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

    if (urlMode == EmbeddingUrlMode::BaseUrl) {
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
        LogService::debug(kLogModule,
                          "embedding request #{} finished but is no longer pending, result dropped",
                          requestId);
        return;
    }

    const PendingRequest request = requestIterator.value();
    pendingRequests.erase(requestIterator);
    request.timer->stop();

    QNetworkReply* reply = request.reply;
    const QByteArray body = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const qint64 elapsedMs = request.elapsed.elapsed();

    if (request.cancelled) {
        LogService::debug(kLogModule, "embedding request #{} aborted by caller after {} ms",
                          requestId, elapsedMs);
        emit embeddingFailed(requestId, EmbeddingError{EmbeddingErrorType::Cancelled,
                                                       QStringLiteral("请求已取消"), httpStatus});
        reply->deleteLater();
        return;
    }

    if (request.timedOut) {
        LogService::warn(kLogModule, "embedding request #{} failed: timeout after {} ms (http {})",
                         requestId, elapsedMs, httpStatus);
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
        LogService::warn(kLogModule, "embedding request #{} failed: http {} in {} ms: {}{}",
                         requestId, httpStatus, elapsedMs, message,
                         request.requestedModel.isEmpty() ? kEmptyModelHint : QString());
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
        LogService::warn(kLogModule, "embedding request #{} failed: network error in {} ms: {}",
                         requestId, elapsedMs, message);
        emit embeddingFailed(requestId,
                             EmbeddingError{EmbeddingErrorType::Network, message, httpStatus});
        reply->deleteLater();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        LogService::warn(kLogModule,
                         "embedding request #{} failed: invalid json response in {} ms: {}",
                         requestId, elapsedMs, parseError.errorString());
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
        LogService::warn(kLogModule,
                         "embedding request #{} failed: response is missing data[0] in {} ms",
                         requestId, elapsedMs);
        emit embeddingFailed(requestId,
                             EmbeddingError{EmbeddingErrorType::InvalidResponse,
                                            QStringLiteral("响应中缺少 data[0]"), httpStatus});
        reply->deleteLater();
        return;
    }

    const QJsonValue embeddingValue =
        dataValue.toArray().first().toObject().value(QStringLiteral("embedding"));
    if (!embeddingValue.isArray() || embeddingValue.toArray().isEmpty()) {
        LogService::warn(kLogModule,
                         "embedding request #{} failed: response carries no usable embedding "
                         "vector in {} ms",
                         requestId, elapsedMs);
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
            LogService::warn(kLogModule,
                             "embedding request #{} failed: embedding vector holds a non-numeric "
                             "element at index {}",
                             requestId, embedding.size());
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

    LogService::debug(kLogModule,
                      "embedding request #{} succeeded: {} dimensions in {} ms (http {}, model: "
                      "'{}')",
                      requestId, embedding.size(), elapsedMs, httpStatus, responseModel);
    emit embeddingSucceeded(requestId, EmbeddingResult{embedding, responseModel});
    reply->deleteLater();
}

void EmbeddingService::failLater(quint64 requestId, EmbeddingErrorType type, const QString& message,
                                 int httpStatus) {
    QTimer::singleShot(0, this, [this, requestId, type, message, httpStatus] {
        LogService::debug(kLogModule, "embedding request #{} rejected: {}", requestId, message);
        emit embeddingFailed(requestId, EmbeddingError{type, message, httpStatus});
    });
}
