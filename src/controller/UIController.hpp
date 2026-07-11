#include <QObject>
#include "service/CopyEventListener.hpp"
#include "service/SQLService.hpp"
#include "struct.hpp"

class UIController : public QObject
{
    Q_OBJECT
private:
    AbstractCopyEventListener* listener;
    SQLService* sql;
    QString currentSearchText;
    SearchMode currentSearchMode = SearchMode::None;

    void refreshCurrentView();

public:
    UIController(QObject* parent = nullptr);
    ~UIController();

    QVector<ContentListItemData> getCopyDate();

private slots:
    void onCopyTrigged();

public slots:
    void requireSearch(const QString& text);

signals:
    void updateUI(QVector<ContentListItemData> data);

};

