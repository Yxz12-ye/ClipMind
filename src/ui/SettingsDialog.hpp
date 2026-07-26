#pragma once

#include <QDialog>

class QEvent;
class QCheckBox;
class QListWidget;
class QStackedWidget;

class CustomHead;

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
    QCheckBox* autoHide;
    QCheckBox* showInTray;

    void setupUI();
    void applyTheme();
};
