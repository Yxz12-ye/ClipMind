#include "MainWindow.hpp"

#include <QDateTime>
#include <QEvent>
#include <QItemSelectionModel>
#include <QPalette>

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

void MainWindow::seedInitialData() {
    if (clipRepository.countClips(ClipFilter{}) > 0) {
        return;
    }

    const auto ensureTag = [this](const TagDraft& draft) {
        const std::optional<Tag> existing = tagRepository.findByNameInsensitive(draft.name);
        if (existing.has_value()) {
            return *existing;
        }

        return tagService.createUserTag(draft);
    };

    const Tag workTag = ensureTag({
        QStringLiteral("工作"),
        QStringLiteral("#3B82F6"),
        10,
        QStringLiteral("和工作协作、会议安排相关的内容。"),
    });
    const Tag repoTag = ensureTag({
        QStringLiteral("仓库"),
        QStringLiteral("#16A34A"),
        20,
        QStringLiteral("和代码仓库、链接收藏相关的内容。"),
    });

    clipRepository.createClip({
        ContentKind::Text,
        QStringLiteral("设计评审会议提前到 15:30，请同步到群里。本条信息已自动同步至云端。"),
        QDateTime::currentDateTimeUtc().addSecs(-90),
        QDateTime::currentDateTimeUtc().addSecs(-90),
        {workTag.id},
    });
    clipRepository.createClip({
        ContentKind::Link,
        QStringLiteral("https://github.com/google-gemini/clipmind"),
        QDateTime::currentDateTimeUtc().addSecs(-12 * 60),
        QDateTime::currentDateTimeUtc().addSecs(-12 * 60),
        {repoTag.id},
    });
    clipRepository.createClip({
        ContentKind::Code,
        QStringLiteral("git clone https://github.com/google-gemini/clipmind.git"),
        QDateTime::currentDateTimeUtc().addSecs(-2 * 60 * 60),
        QDateTime::currentDateTimeUtc().addSecs(-2 * 60 * 60),
        {repoTag.id, workTag.id},
    });
}

void MainWindow::reloadTagFilters() {
    tagModel.setItems(tagService.buildFilterItems());
    if (tagModel.rowCount() == 0) {
        return;
    }

    const QModelIndex firstItem = tagModel.index(0, 0);
    tagListView.setCurrentIndex(firstItem);
    if (tagListView.selectionModel() != nullptr) {
        tagListView.selectionModel()->select(firstItem, QItemSelectionModel::ClearAndSelect);
    }
}

namespace {
QString formatRelativeTime(const QDateTime& createdAt) {
    if (!createdAt.isValid()) {
        return QStringLiteral("刚刚");
    }

    const qint64 seconds = createdAt.secsTo(QDateTime::currentDateTimeUtc());
    if (seconds < 60) {
        return QStringLiteral("刚刚");
    }
    if (seconds < 3600) {
        return QStringLiteral("%1 分钟前").arg(seconds / 60);
    }
    if (seconds < 86400) {
        return QStringLiteral("%1 小时前").arg(seconds / 3600);
    }

    return QStringLiteral("%1 天前").arg(seconds / 86400);
}
}

void MainWindow::refreshContentList() {
    const QVector<ClipRecord> clips = clipRepository.listClips(currentFilter);
    QVector<ContentListItemData> items;
    items.reserve(clips.size());

    for (int index = 0; index < clips.size(); ++index) {
        const ClipRecord& clip = clips.at(index);
        items.push_back({
            clip.kind,
            formatRelativeTime(clip.createdAt),
            clip.content,
            index == 0,
        });
    }

    contentList.setItems(items);
}

void MainWindow::handleTagSelectionChanged(const QModelIndex& current, const QModelIndex& previous) {
    Q_UNUSED(previous);

    currentFilter = ClipFilter{};
    const TagFilterItem* item = tagModel.itemAt(current);
    if (item == nullptr) {
        refreshContentList();
        return;
    }

    if (item->kind == TagFilterKind::BuiltinKind) {
        currentFilter.kind = item->contentKind;
    } else if (item->kind == TagFilterKind::UserTag) {
        currentFilter.tagId = item->tagId;
    }

    refreshContentList();
}

MainWindow::MainWindow()
    : central(this),
      head(&central),
      searchWidget(&central),
      tagContainer(&central),
      tagListView(&central),
      contentList(&central),
      tagContainerLayout(&tagContainer),
      layout(&central),
      database(),
      tagRepository(database),
      clipRepository(database),
      tagService(tagRepository, clipRepository),
      tagModel() {
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
    tagContainerLayout.addStretch();
    tagContainerLayout.addWidget(&tagListView, 0, Qt::AlignCenter);
    tagContainerLayout.addStretch();

    tagListView.setModel(&tagModel);
    tagListView.setSelectionMode(QAbstractItemView::SingleSelection);
    tagListView.setStyleSheet("QListView { background: transparent; }");

    applyTheme();
    setCentralWidget(&central);
    connect(&head, &CustomHead::closeRequested, this, &QWidget::close);
    connect(&head, &CustomHead::moveRequested, this, [=](QPoint pos){move(pos);});
    setupUI();
    seedInitialData();
    reloadTagFilters();
    connect(
        tagListView.selectionModel(),
        &QItemSelectionModel::currentChanged,
        this,
        &MainWindow::handleTagSelectionChanged);
    handleTagSelectionChanged(tagModel.index(0, 0), QModelIndex{});
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        applyTheme();
    }

    QMainWindow::changeEvent(event);
}
