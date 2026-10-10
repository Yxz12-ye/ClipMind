#include "UIController.hpp"

#include <QTimer>

#include "service/LogService.hpp"
#include "service/SettingService.hpp"

namespace {

QString embeddingModelKey(const EmbeddingConfig& config, const EmbeddingResult& result) {
    // Configured aliases remain stable; unnamed server defaults fall back to an endpoint key.
    QString model = config.model.trimmed();
    if (model.isEmpty()) {
        model = result.model.trimmed();
    }
    if (model.isEmpty()) {
        model = QStringLiteral("default@%1:%2")
                    .arg(config.urlMode == EmbeddingUrlMode::BaseUrl ? QStringLiteral("base")
                                                                     : QStringLiteral("endpoint"),
                         config.url.trimmed());
    }
    return model;
}

}  // namespace

UIController::UIController(QObject* parent)
    : UIController(createCopyEventListener(), new EmbeddingService(), new SQLService(), parent) {
    if (listener != nullptr) {
        listener->setParent(this);
    }
    embedding->setParent(this);
    sql->setParent(this);
    connect(SettingService::instance(), &SettingService::valueChanged, this,
            [this](const QString& key) {
                if (key == QLatin1String("core/embeddingUrl") ||
                    key == QLatin1String("core/embeddingModel") ||
                    key == QLatin1String("core/embeddingUrlMode")) {
                    reloadEmbeddingConfig();
                }
            });
}

UIController::UIController(AbstractCopyEventListener* listener, EmbeddingService* embedding,
                           SQLService* sql, QObject* parent)
    : QObject(parent), listener(listener), embedding(embedding), sql(sql) {
    Q_ASSERT(embedding != nullptr && sql != nullptr);
    if (listener != nullptr) {
        connect(listener, &AbstractCopyEventListener::clipboardChanged, this,
                &UIController::onCopyTrigged);
    }
    connect(embedding, &EmbeddingService::embeddingSucceeded, this,
            [this](quint64 requestId, const EmbeddingResult& result) {
                if (requestId == semanticRequestId) {
                    semanticRequestId = 0;
                    semanticEmbedding = result.embedding;
                    semanticModel = embeddingModelKey(semanticConfig, result);
                    refreshCurrentView();
                    return;
                }
                auto it = pendingEmbeddings.find(requestId);
                if (it == pendingEmbeddings.end()) {
                    return;
                }
                const PendingEmbedding pending = it.value();
                pendingEmbeddings.erase(it);
                const QString model = embeddingModelKey(pending.config, result);
                const QString error =
                    this->sql->saveEmbedding(pending.hash, model, result.embedding);
                if (!error.isEmpty()) {
                    LogService::warn("UIController", "save embedding request #{} failed: {}",
                                     requestId, error);
                } else if (currentSearchMode == SearchMode::Semantics) {
                    refreshCurrentView();
                }
            });
    connect(embedding, &EmbeddingService::embeddingFailed, this,
            [this](quint64 requestId, const EmbeddingError& error) {
                if (requestId == semanticRequestId) {
                    semanticRequestId = 0;
                    LogService::warn("UIController", "semantic search request #{} failed: {}",
                                     requestId, error.message);
                    emit updateUI({});
                    return;
                }
                if (pendingEmbeddings.remove(requestId) != 0) {
                    LogService::warn("UIController", "clipboard embedding request #{} failed: {}",
                                     requestId, error.message);
                }
            });
}

UIController::~UIController() {
    resetSemanticSearch();
    const auto requests = pendingEmbeddings.keys();
    pendingEmbeddings.clear();
    for (quint64 requestId : requests) {
        embedding->cancelRequest(requestId);
    }
}

void UIController::reloadEmbeddingConfig() {
    const SettingService* settings = SettingService::instance();
    EmbeddingConfig config;
    config.url = settings->get(QStringLiteral("core/embeddingUrl")).toString();
    config.model = settings->get(QStringLiteral("core/embeddingModel")).toString();
    config.urlMode =
        settings->get(QStringLiteral("core/embeddingUrlMode")).toString() == QLatin1String("base")
            ? EmbeddingUrlMode::BaseUrl
            : EmbeddingUrlMode::FullEndpoint;
    if (config != embedding->config()) {
        embedding->setConfig(config);
        resetSemanticSearch();
        if (currentSearchMode == SearchMode::Semantics) {
            refreshCurrentView();
        }
    }
}

void UIController::resetSemanticSearch() {
    const quint64 requestId = semanticRequestId;
    semanticRequestId = 0;
    semanticEmbedding.clear();
    semanticModel.clear();
    semanticConfig = {};
    if (requestId != 0) {
        embedding->cancelRequest(requestId);
    }
}

void UIController::refreshSemanticSearch() {
    const EmbeddingConfig config = embedding->config();
    if (semanticConfig != config) {
        resetSemanticSearch();
        semanticConfig = config;
    }
    if (semanticEmbedding.isEmpty()) {
        if (semanticRequestId != 0) {
            return;
        }
        emit updateUI({});
        if (config.url.trimmed().isEmpty()) {
            LogService::warn("UIController", "semantic search requires an embedding endpoint");
            return;
        }
        semanticRequestId = embedding->embedText(currentSearchText);
        return;
    }

    QString error;
    const auto matches =
        sql->searchByEmbedding(semanticEmbedding, semanticModel, currentTagName, 100, &error);
    if (!error.isEmpty()) {
        LogService::warn("UIController", "semantic search failed: {}", error);
    }
    QVector<ContentListItemData> items;
    items.reserve(matches.size());
    LogService::debug("UIController", "semantic search: {} matches (model: '{}', tag: '{}')",
                      matches.size(), semanticModel, currentTagName);
    qsizetype rank = 0;
    for (const auto& match : matches) {
        LogService::debug(
            "UIController",
            "semantic match #{}: similarity={:.2f}%, distance={:.6f}, hash={}, content='{}'",
            ++rank, (1.0 - match.distance) * 100.0, match.distance,
            QString::fromLatin1(match.item.hash.toHex()), match.item.content.left(80).simplified());
        items.push_back(match.item);
    }
    emit updateUI(items);
}

void UIController::embedContent(const ContentListItemData& data) {
    const EmbeddingConfig config = embedding->config();
    if (config.url.trimmed().isEmpty()) {
        return;
    }
    const QString model = config.model.trimmed();
    if (!model.isEmpty()) {
        QString error;
        const auto stored = sql->getEmbedding(data.hash, model, &error);
        if (!error.isEmpty()) {
            LogService::warn("UIController", "read stored embedding failed: {}", error);
            return;
        }
        if (!stored.isEmpty()) {
            return;
        }
    }
    for (const auto& pending : pendingEmbeddings) {
        if (pending.hash == data.hash && pending.config == config) {
            return;
        }
    }
    const quint64 requestId = embedding->embedText(data.content);
    pendingEmbeddings.insert(requestId, PendingEmbedding{data.hash, config});
}

QVector<ContentListItemData> UIController::getCopyDate() {
    return sql->get();
}

QVector<Tag> UIController::getTags() const {
    return sql->getTags();
}

void UIController::refreshCurrentView() {
    if (currentSearchMode == SearchMode::Semantics && !currentSearchText.trimmed().isEmpty()) {
        refreshSemanticSearch();
        return;
    }
    if (!currentTagName.isEmpty()) {
        Tag tag;
        tag.tagName = currentTagName;
        emit updateUI(sql->search(currentSearchText, QString(), tag, currentSearchMode));
        return;
    }

    if (currentSearchText.trimmed().isEmpty()) {
        emit updateUI(sql->get());
        return;
    }

    emit updateUI(sql->search(currentSearchText, currentSearchMode));
}

void UIController::onCopyTrigged() {
    const QString content = listener->text();
    if (content.trimmed().isEmpty()) {
        return;
    }
    const Tag tag = sql->matchTag(content);
    ContentListItemData data(tag, content, QDateTime::currentDateTime(),
                             QDateTime::currentDateTime());
    const QString error = sql->save(data);
    if (!error.isEmpty()) {
        LogService::warn("UIController", "save clipboard content failed: {}", error);
        return;
    }
    refreshCurrentView();
    embedContent(data);
}

void UIController::requireSearch(const QString& text) {
    const bool semantic = text.startsWith(QLatin1String("# "));
    const SearchMode mode = semantic ? SearchMode::Semantics : SearchMode::Regex;
    const QString query = semantic ? text.mid(2).trimmed() : text;
    if (currentSearchMode != mode || currentSearchText != query) {
        resetSemanticSearch();
    }
    currentSearchText = query;
    currentSearchMode = mode;
    refreshCurrentView();
}

void UIController::requireTagFilter(const QString& tagName) {
    currentTagName = tagName;
    refreshCurrentView();
}

void UIController::pasteContent(const QString& text) {
    if (listener == nullptr) {
        return;
    }

    emit hideWindowRequested();
    AbstractCopyEventListener* pasteListener = listener;
    QTimer::singleShot(0, this, [this, pasteListener, text] {
        if (pasteListener->pasteText(text)) {
            sql->updateContentTime(text);
            refreshCurrentView();
        }
    });
}

void UIController::setItemPinned(const QByteArray& hash, bool pinned) {
    if (sql->setPinned(hash, pinned)) {
        refreshCurrentView();
    }
}

void UIController::deleteItem(const QByteArray& hash) {
    if (sql->deleteItem(hash)) {
        QVector<quint64> requests;
        for (auto it = pendingEmbeddings.begin(); it != pendingEmbeddings.end();) {
            if (it->hash == hash) {
                requests.push_back(it.key());
                it = pendingEmbeddings.erase(it);
            } else {
                ++it;
            }
        }
        // Remove tracking before abort(), which can synchronously emit embeddingFailed.
        for (quint64 requestId : requests) {
            embedding->cancelRequest(requestId);
        }
        refreshCurrentView();
    }
}

void UIController::changeItemTag(const QByteArray& hash, const QString& tagName) {
    if (sql->updateItemTag(hash, tagName)) {
        refreshCurrentView();
    }
}
