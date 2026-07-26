#pragma once

#include <QDialog>

class QEvent;
class QCheckBox;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;

class CustomHead;
struct Tag;

class SettingsDialog : public QDialog {
public:
    SettingsDialog(bool hideAfterPaste, bool showTrayIcon, QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    bool hideAfterPasteEnabled() const;
    bool trayIconEnabled() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    CustomHead* head;
    QListWidget* categories;
    QStackedWidget* pages;
    QListWidget* tagList;
    QCheckBox* autoHide;
    QCheckBox* showInTray;

    void setupUI();
    void applyTheme();
    void addTag();
    void addTagItem(const Tag& tag);
    void removeTagItem(QListWidgetItem* item);
    void moveTagItem(QListWidgetItem* item, int offset);
};
