#include "UIController.hpp"

#include <QTimer>

#include "service/LogService.hpp"
#include "service/SettingService.hpp"

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
                auto it = pendingEmbeddings.find(requestId);
                if (it == pendingEmbeddings.end()) {
                    return;
                }
                const PendingEmbedding pending = it.value();
                pendingEmbeddings.erase(it);
                // Use the configured identifier for aliases. Scope unnamed server defaults to URL.
                QString model = pending.config.model.trimmed();
                if (model.isEmpty()) {
                    model = result.model.trimmed();
                }
                if (model.isEmpty()) {
                    model = QStringLiteral("default@%1:%2")
                                .arg(pending.config.urlMode == EmbeddingUrlMode::BaseUrl
                                         ? QStringLiteral("base")
                                         : QStringLiteral("endpoint"),
                                     pending.config.url.trimmed());
                }
                const QString error =
                    this->sql->saveEmbedding(pending.hash, model, result.embedding);
                if (!error.isEmpty()) {
                    LogService::warn("UIController", "save embedding request #{} failed: {}",
                                     requestId, error);
                }
            });
    connect(embedding, &EmbeddingService::embeddingFailed, this,
            [this](quint64 requestId, const EmbeddingError& error) {
                if (pendingEmbeddings.remove(requestId) != 0) {
                    LogService::warn("UIController", "clipboard embedding request #{} failed: {}",
                                     requestId, error.message);
                }
            });
}

UIController::~UIController() {
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
    embedding->setConfig(config);
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
    currentSearchText = text;
    currentSearchMode = SearchMode::None;
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
