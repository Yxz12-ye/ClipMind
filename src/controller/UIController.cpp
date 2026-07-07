#include "UIController.hpp"

UIController::UIController(QObject* parent)
    : QObject(parent), listener(createCopyEventListener(this)), sql(new SQLService(this)) 
{
    connect(listener, &AbstractCopyEventListener::clipboardChanged, this, &UIController::onCopyTrigged);
}

UIController::~UIController() {}

QVector<ContentListItemData> UIController::getCopyDate() {
    return sql->get();
}

void UIController::onCopyTrigged() {
    ContentListItemData data(Tag{"TEXT","",SearchMode::None}, listener->text(), QDateTime::currentDateTime(), QDateTime::currentDateTime());
    sql->save(data);
    emit updateUI(sql->get());
}