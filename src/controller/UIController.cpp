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

void UIController::refreshCurrentView() {
    if (currentSearchText.trimmed().isEmpty()) {
        emit updateUI(sql->get());
        return;
    }

    emit updateUI(sql->search(currentSearchText, currentSearchMode));
}

void UIController::onCopyTrigged() {
    ContentListItemData data(Tag{"TEXT", "", SearchMode::None}, listener->text(),
                             QDateTime::currentDateTime(), QDateTime::currentDateTime());
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
    QTimer::singleShot(0, pasteListener, [pasteListener, text] { pasteListener->pasteText(text); });
}
