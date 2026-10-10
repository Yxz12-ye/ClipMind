#pragma once

#include <QHash>
#include <QObject>

#include "service/CopyEventListener.hpp"
#include "service/EmbeddingService.hpp"
#include "service/SQLService.hpp"
#include "struct.hpp"

class UIController : public QObject {
    Q_OBJECT
private:
    AbstractCopyEventListener* listener;
    EmbeddingService* embedding;
    SQLService* sql;
    QString currentSearchText;
    QString currentTagName;
    SearchMode currentSearchMode = SearchMode::None;

    struct PendingEmbedding {
        QByteArray hash;
        EmbeddingConfig config;
    };
    QHash<quint64, PendingEmbedding> pendingEmbeddings;
    quint64 semanticRequestId = 0;
    EmbeddingConfig semanticConfig;
    QVector<float> semanticEmbedding;
    QString semanticModel;

    void refreshCurrentView();
    void refreshSemanticSearch();
    void resetSemanticSearch();
    void embedContent(const ContentListItemData& data);

public:
    UIController(QObject* parent = nullptr);
    // Injected services remain owned by the caller and must outlive the controller.
    UIController(AbstractCopyEventListener* listener, EmbeddingService* embedding, SQLService* sql,
                 QObject* parent = nullptr);
    ~UIController();

    QVector<ContentListItemData> getCopyDate();
    QVector<Tag> getTags() const;

    // 供设置页等直接访问数据库完成标签持久化
    SQLService* sqlService() const { return sql; }
    EmbeddingService* embeddingService() const { return embedding; }
    void reloadEmbeddingConfig();

private slots:
    void onCopyTrigged();

public slots:
    void requireSearch(const QString& text);
    void requireTagFilter(const QString& tagName);
    void pasteContent(const QString& text);
    void setItemPinned(const QByteArray& hash, bool pinned);
    void deleteItem(const QByteArray& hash);
    void changeItemTag(const QByteArray& hash, const QString& tagName);

signals:
    void updateUI(QVector<ContentListItemData> data);
    void hideWindowRequested();
};
