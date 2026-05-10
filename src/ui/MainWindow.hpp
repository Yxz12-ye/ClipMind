#pragma once

#include <QMainWindow>
#include <QModelIndex>
#include <QWidget>

#include "data/SQLiteClipRepository.hpp"
#include "data/SQLiteDatabase.hpp"
#include "data/SQLiteTagRepository.hpp"
#include "service/TagService.hpp"
#include "ui/ContentListWidget.hpp"
#include "ui/CustomHead.hpp"
#include "ui/SearchWidget.hpp"
#include "ui/TagFilterModel.hpp"
#include "ui/TagWidget.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
private:
    QWidget central;
    CustomHead head;
    SearchWidget searchWidget;
    QWidget tagContainer;
    TagListView tagListView;
    ContentListWidget contentList;
    QHBoxLayout tagContainerLayout;
    QVBoxLayout layout;
    SQLiteDatabase database;
    SQLiteTagRepository tagRepository;
    SQLiteClipRepository clipRepository;
    TagService tagService;
    TagFilterModel tagModel;
    ClipFilter currentFilter;

    void setupUI();
    void applyTheme();
    void seedInitialData();
    void reloadTagFilters();
    void refreshContentList();
    void handleTagSelectionChanged(const QModelIndex& current, const QModelIndex& previous);

public:
    MainWindow(/* args */);
    ~MainWindow()=default;

protected:
    void changeEvent(QEvent* event) override;
};
