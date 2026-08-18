#pragma once

#include <QDialog>
#include <QElapsedTimer>

#include "service/EmbeddingService.hpp"

class QEvent;
class QCheckBox;
class QComboBox;
class QColor;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;
class SQLService;

class CustomHead;
struct Tag;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(SQLService* service, EmbeddingService* embeddingService, bool hideAfterPaste,
                   bool showTrayIcon, const EmbeddingConfig& embeddingConfig,
                   QWidget* parent = nullptr);
    ~SettingsDialog() override;

    bool hideAfterPasteEnabled() const;
    bool trayIconEnabled() const;
    EmbeddingConfig embeddingConfig() const;

signals:
    void settingsChanged(bool hideAfterPaste, bool showTrayIcon,
                         const EmbeddingConfig& embeddingConfig);

protected:
    void changeEvent(QEvent* event) override;

private:
    SQLService* service;
    EmbeddingService* embedding;
    CustomHead* head;
    QListWidget* categories;
    QStackedWidget* pages;
    QListWidget* tagList;
    QCheckBox* autoHide;
    QCheckBox* showInTray;
    QComboBox* embeddingUrlMode;
    QLineEdit* embeddingUrl;
    QLineEdit* embeddingModel;
    QPushButton* embeddingTestButton;
    QLabel* embeddingTestStatus;
    QElapsedTimer embeddingTestTimer;
    quint64 embeddingRequestId = 0;

    void setupUI();
    void applyTheme();
    void addTag();
    void addTagItem(const Tag& tag);
    void updateTagItem(QListWidgetItem* item, const Tag& tag);
    void editTag(QListWidgetItem* item);
    Tag tagFromItem(QListWidgetItem* item) const;
    bool containsTag(const QString& name) const;
    void removeTagItem(QListWidgetItem* item);
    void moveTagItem(QListWidgetItem* item, int offset);
    void testEmbedding();
    void setEmbeddingStatus(const QString& text, const QColor& color);
    void notifySettingsChanged();
};
