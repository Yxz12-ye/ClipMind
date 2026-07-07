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

public:
    UIController(QObject* parent = nullptr);
    ~UIController();

    QVector<ContentListItemData> getCopyDate();

private slots:
    void onCopyTrigged();

signals:
    void updateUI(QVector<ContentListItemData> data);

};

