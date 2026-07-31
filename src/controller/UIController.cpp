#include "UIController.hpp"

#include <QTimer>

UIController::UIController(QObject* parent)
    : QObject(parent), listener(createCopyEventListener(this)), sql(new SQLService(this)) {
    connect(listener, &AbstractCopyEventListener::clipboardChanged, this,
            &UIController::onCopyTrigged);
}

UIController::~UIController() {}

QVector<ContentListItemData> UIController::getCopyDate() {
    return sql->get();
}

QVector<Tag> UIController::getTags() const {
    return sql->getTags();
}

void UIController::refreshCurrentView() {
    if (currentSearchText.trimmed().isEmpty()) {
        emit updateUI(sql->get());
        return;
    }

    emit updateUI(sql->search(currentSearchText, currentSearchMode));
}

void UIController::onCopyTrigged() {
    const QString content = listener->text();
    const Tag tag = sql->matchTag(content);
    ContentListItemData data(tag, content, QDateTime::currentDateTime(),
                             QDateTime::currentDateTime());
    sql->save(data);
    refreshCurrentView();
}

void UIController::requireSearch(const QString& text) {
    currentSearchText = text;
    currentSearchMode = SearchMode::None;
    refreshCurrentView();
}

void UIController::pasteContent(const QString& text) {
    if (listener == nullptr) {
        return;
    }

    emit hideWindowRequested();
    AbstractCopyEventListener* pasteListener = listener;
    QTimer::singleShot(0, pasteListener, [this, pasteListener, text] {
        if (pasteListener->pasteText(text)) {
            sql->updateContentTime(text);
            refreshCurrentView();
        }
    });
}
