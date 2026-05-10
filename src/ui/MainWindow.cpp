#include "MainWindow.hpp"

#include <QEvent>
#include <QItemSelectionModel>
#include <QPalette>
#include <QStandardItem>
#include <QStringList>

void MainWindow::setupUI() {
    layout.setContentsMargins(QMargins(0,0,0,0));
    layout.setSpacing(0);
    layout.addWidget(&head);
    layout.addSpacing(8);
    layout.addWidget(&searchWidget);
    layout.addSpacing(12);
    layout.addWidget(&tagContainer, 0, Qt::AlignHCenter);
    layout.addSpacing(12);
    layout.addWidget(&contentList, 1);
}

void MainWindow::applyTheme() {
    const bool darkMode = palette().color(QPalette::Window).lightness() < 128;
    const QString background = darkMode ? "#1C1C1C" : "#FFFFFF";
    const QString hover = darkMode ? "rgba(255, 255, 255, 0.08)" : "rgba(15, 23, 42, 0.06)";

    central.setStyleSheet(QString(
        "QWidget#centralPanel {"
        "background-color: %1;"
        "border-radius: 12px;"
        "}"
        "QWidget#centralPanel QToolButton {"
        "border: none;"
        "border-radius: 9px;"
        "background: transparent;"
        "}"
        "QWidget#centralPanel QToolButton:hover {"
        "background-color: %2;"
        "}"
    ).arg(background, hover));
}

void MainWindow::populateDemoData() {
    model->clear();
    const std::vector<Tag> taglist = {{"TEXT","文本"}, {"CODE","代码"},{"LINK","链接"}};
    for (const Tag& tag : taglist) {
        model->appendRow(new QStandardItem(tag.displayName));
    }
    const QModelIndex firstTag = model->index(0, 0);
    tagListView.setCurrentIndex(firstTag);
    tagListView.selectionModel()->select(firstTag, QItemSelectionModel::ClearAndSelect);

    contentList.setItems({
        {SysContentItemKind::Text, "刚刚", "设计评审会议提前到 15:30，请同步到群里。本条信息已自动同步至云端。", true},
        {SysContentItemKind::Link, "12 分钟前", "https://github.com/google-gemini/clipmind"},
        {SysContentItemKind::Code, "2 小时前", "git clone https://github.com/google-gemini/clipmind.git"},
    });
}

MainWindow::MainWindow()
    : central(this),
      head(&central),
      searchWidget(&central),
      tagContainer(&central),
      tagListView(&central),
      contentList(&central),
      model(new QStandardItemModel(this)),
      tagContainerLayout(&tagContainer),
      layout(&central) {
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(360, 400);
    central.setObjectName("centralPanel");
    central.setAttribute(Qt::WA_StyledBackground, true);

    tagContainer.setFixedSize(328, 28);
    tagContainer.setAttribute(Qt::WA_StyledBackground, true);
    tagContainer.setStyleSheet("QWidget { background: transparent; }");
    tagContainerLayout.setContentsMargins(0, 0, 0, 0);
    tagContainerLayout.setSpacing(0);
    tagContainerLayout.addWidget(&tagListView, 0, Qt::AlignCenter);
    tagContainerLayout.addStretch();

    tagListView.setModel(model);
    tagListView.setSelectionMode(QAbstractItemView::SingleSelection);
    tagListView.setStyleSheet("QListView { background: transparent; }");
    populateDemoData();

    applyTheme();
    setCentralWidget(&central);
    connect(&head, &CustomHead::closeRequested, this, &QWidget::close);
    connect(&head, &CustomHead::moveRequested, this, [=](QPoint pos){move(pos);});
    setupUI();
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        applyTheme();
    }

    QMainWindow::changeEvent(event);
}
