#include <QObject>

#include "service/CopyEventListener.hpp"
#include "service/SQLService.hpp"
#include "struct.hpp"

class UIController : public QObject {
    Q_OBJECT
private:
    AbstractCopyEventListener* listener;
    SQLService* sql;
    QString currentSearchText;
    QString currentTagName;
    SearchMode currentSearchMode = SearchMode::None;

    void refreshCurrentView();

public:
    UIController(QObject* parent = nullptr);
    ~UIController();

    QVector<ContentListItemData> getCopyDate();
    QVector<Tag> getTags() const;

    // 供设置页等直接访问数据库完成标签持久化
    SQLService* sqlService() const { return sql; }

private slots:
    void onCopyTrigged();

public slots:
    void requireSearch(const QString& text);
    void requireTagFilter(const QString& tagName);
    void pasteContent(const QString& text);

signals:
    void updateUI(QVector<ContentListItemData> data);
    void hideWindowRequested();
};
