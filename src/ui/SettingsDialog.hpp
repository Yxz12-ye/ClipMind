#pragma once

#include <QDialog>
#include <QHash>

class QEvent;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class SQLService;
class SettingEditor;
class SettingRegistry;

class CustomHead;
struct Tag;

class SettingsDialog : public QDialog {
public:
    SettingsDialog(SQLService* service, SettingRegistry* registry, QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

protected:
    void changeEvent(QEvent* event) override;

private:
    SQLService* service = nullptr;
    SettingRegistry* registry = nullptr;
    CustomHead* head;
    QListWidget* categories;
    QStackedWidget* pages;
    QListWidget* tagList;
    QHash<QString, SettingEditor*> editors;

    void setupUI();
    void applyTheme();
    QWidget* createStandardPage(const QString& pageId, QWidget* parent);
    QWidget* createTagPage(QWidget* parent);
    QWidget* createShortcutPage(QWidget* parent);
    QWidget* createAppearancePage(QWidget* parent);
    QWidget* createAboutPage(QWidget* parent);
    SettingEditor* createEditor(const struct SettingDefinition& setting, QWidget* parent);
    void bindEditor(const struct SettingDefinition& setting, SettingEditor* editor);
    void addTag();
    void addTagItem(const Tag& tag);
    void updateTagItem(QListWidgetItem* item, const Tag& tag);
    void editTag(QListWidgetItem* item);
    Tag tagFromItem(QListWidgetItem* item) const;
    bool containsTag(const QString& name) const;
    void removeTagItem(QListWidgetItem* item);
    void moveTagItem(QListWidgetItem* item, int offset);
};
