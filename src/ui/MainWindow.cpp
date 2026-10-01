#include "MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QIcon>
#include <QItemSelectionModel>
#include <QPalette>
#include <QStandardItem>
#include <QStringList>

#include "config.hpp"
#include "controller/SettingsController.hpp"
#include "service/SettingRegistry.hpp"
#include "service/SettingService.hpp"

#ifdef Q_OS_WIN
#include <WinUser.h>
#include <windows.h>
#endif

void MainWindow::setupUI() {
    layout.setContentsMargins(QMargins(0, 0, 0, 0));
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

    central.setStyleSheet(QString("QWidget#centralPanel {"
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
                                  "}")
                              .arg(background, hover));
}

void MainWindow::refreshTagBar() {
    model->clear();

    auto* allItem = new QStandardItem(QStringLiteral("ALL"));
    allItem->setEditable(false);
    allItem->setToolTip(QStringLiteral("显示全部内容"));
    allItem->setData(QString(), TagBarNameRole);
    model->appendRow(allItem);

    const QVector<Tag> tags = controller->getTags();
    for (const Tag& tag : tags) {
        auto* item = new QStandardItem(tag.tagName);
        item->setEditable(false);
        item->setToolTip(tag.rule.isEmpty() ? tag.tagName
                                            : QStringLiteral("%1\n%2").arg(tag.tagName, tag.rule));
        item->setData(tag.tagBackColor, TagBarBackgroundRole);
        item->setData(tag.tagNameColor, TagBarForegroundRole);
        item->setData(tag.tagName, TagBarNameRole);
        model->appendRow(item);
    }

    tagListView.selectionModel()->setCurrentIndex(model->index(0, 0),
                                                  QItemSelectionModel::ClearAndSelect);
}

void MainWindow::resetTagFilter() {
    if (model->rowCount() > 0) {
        tagListView.selectionModel()->setCurrentIndex(model->index(0, 0),
                                                      QItemSelectionModel::ClearAndSelect);
    }
    controller->requireTagFilter(QString());
}

void MainWindow::setupTray() {
    const QIcon appIcon(":/img/icon.svg");
    setWindowIcon(appIcon);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    trayIcon.setIcon(appIcon);
    trayIcon.setToolTip(hotkeyLabel.isEmpty() ? QStringLiteral("ClipMind")
                                              : QStringLiteral("ClipMind (%1)").arg(hotkeyLabel));

    QAction* settingsAction = trayMenu.addAction(QStringLiteral("设置"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    trayMenu.addSeparator();
    QAction* exitAction = trayMenu.addAction(QStringLiteral("退出"));
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitFromTray);
    connect(&trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                    showWindow();
                }
            });

    trayIcon.setContextMenu(&trayMenu);
    if (showTrayIcon) {
        trayIcon.show();
    }
}

void MainWindow::setupGlobalHotkey() {
#ifdef Q_OS_WIN
    const WId windowId = winId();
    HWND hwnd = reinterpret_cast<HWND>(windowId);
    if (hwnd == nullptr) {
        return;
    }

    struct HotkeyCandidate {
        unsigned int modifiers;
        unsigned int virtualKey;
        const wchar_t* label;
    };

    const HotkeyCandidate candidates[] = {
        {MOD_ALT | MOD_NOREPEAT, 'V', L"Alt+V"},
        {MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'V', L"Ctrl+Alt+V"},
    };

    for (const HotkeyCandidate& candidate : candidates) {
        if (RegisterHotKey(hwnd, kHotkeyId, candidate.modifiers, candidate.virtualKey)) {
            hotkeyRegistered = true;
            hotkeyModifiers = candidate.modifiers;
            hotkeyVirtualKey = candidate.virtualKey;
            hotkeyLabel = QString::fromWCharArray(candidate.label);
            return;
        }
    }
#endif
}

void MainWindow::teardownGlobalHotkey() {
#ifdef Q_OS_WIN
    if (!hotkeyRegistered) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd != nullptr) {
        UnregisterHotKey(hwnd, kHotkeyId);
    }
#endif

    hotkeyRegistered = false;
    hotkeyModifiers = 0;
    hotkeyVirtualKey = 0;
    hotkeyLabel.clear();
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
      layout(&central),
      trayMenu(this),
      trayIcon(this),
      controller(new UIController(this)),
      settingRegistry(new SettingRegistry(this)),
      settingsController(new SettingsController(settingRegistry, controller->sqlService(),
                                                SettingService::instance(),
                                                controller->embeddingService(), this)),
      windowPositioner(createWindowPositioner()) {
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(360, 400);
    central.setObjectName("centralPanel");
    central.setAttribute(Qt::WA_StyledBackground, true);

    auto coreSettings = settingRegistry->registerPlugin(QStringLiteral("core"));
    coreSettings.registerPage(QStringLiteral("general"), QStringLiteral("通用"),
                              QStringLiteral("管理窗口和剪贴板的默认行为"), 0);
    coreSettings.registerGroup(QStringLiteral("general"), QStringLiteral("behavior"),
                               QStringLiteral("窗口行为"));
    coreSettings.registerBool(QStringLiteral("behavior"), QStringLiteral("hideAfterPaste"),
                              QStringLiteral("粘贴后收起窗口"),
                              QStringLiteral("选择剪贴板内容并粘贴后自动隐藏主窗口"), true);
    coreSettings.registerBool(QStringLiteral("behavior"), QStringLiteral("showTrayIcon"),
                              QStringLiteral("通知区域图标"),
                              QStringLiteral("在系统通知区域保留 ClipMind 图标"), true);

    coreSettings.registerPage(QStringLiteral("tags"), QStringLiteral("标签管理"),
                              QStringLiteral("标签显示与自动匹配顺序"), 10);
    // 向量化服务原来的设置项直接写在 SettingsDialog 里, 重构后统一改成在这里注册定义,
    // 设置页只是把这些定义渲染出来(见 SettingRegistry)
    coreSettings.registerPage(QStringLiteral("embedding"), QStringLiteral("向量化"),
                              QStringLiteral("配置 OpenAI 兼容的 Embeddings 接口"), 15);
    coreSettings.registerGroup(QStringLiteral("embedding"), QStringLiteral("service"),
                               QStringLiteral("服务配置"));
    coreSettings.registerEnum(
        QStringLiteral("service"), QStringLiteral("embeddingUrlMode"), QStringLiteral("URL 类型"),
        QStringLiteral("完整接口填写到 /embeddings；Base URL 填写到版本路径，例如 /v1"),
        {{QStringLiteral("full"), QStringLiteral("完整接口")},
         {QStringLiteral("base"), QStringLiteral("Base URL")}},
        QStringLiteral("full"));
    coreSettings.registerString(QStringLiteral("service"), QStringLiteral("embeddingUrl"),
                                QStringLiteral("接口 URL"),
                                QStringLiteral("服务端接收向量化请求的地址"), QString());
    coreSettings.registerString(QStringLiteral("service"), QStringLiteral("embeddingModel"),
                                QStringLiteral("模型"),
                                QStringLiteral("发送给接口的模型标识, 留空则把空标识发给接口, "
                                               "由服务端套用默认模型"),
                                QString());
    // 动作项: 按钮文本由注册表给出, 点击后由 SettingsController 执行接口测试
    coreSettings.registerAction(QStringLiteral("service"), QStringLiteral("embeddingTest"),
                                QStringLiteral("发送测试文本"),
                                QStringLiteral("使用固定文本验证接口连通性并检查向量响应"),
                                QStringLiteral("测试接口"));
    coreSettings.registerPage(QStringLiteral("shortcuts"), QStringLiteral("快捷键"),
                              QStringLiteral("快速呼出 ClipMind"), 20);
    coreSettings.registerPage(QStringLiteral("appearance"), QStringLiteral("外观"),
                              QStringLiteral("界面随系统主题自动调整"), 30);
    coreSettings.registerPage(QStringLiteral("about"), QStringLiteral("关于"),
                              QStringLiteral("ClipMind 剪贴板管理器"), 40);
    coreSettings.registerGroup(QStringLiteral("about"), QStringLiteral("version"),
                               QStringLiteral("版本信息"));
    // 只读且不写配置文件: 版本号来自 CMake 的 PROJECT_VERSION(见根 CMakeLists.txt)
    coreSettings.registerFixedText(QStringLiteral("version"), QStringLiteral("softwareVersion"),
                                   QStringLiteral("软件版本"),
                                   QStringLiteral("当前安装的 ClipMind 版本号"),
                                   QStringLiteral(PROJECT_VERSION));
    // Plugin modules register their settings before the registry is sealed.
    settingRegistry->seal();

    // 提交注册信息: 注册阶段只收集定义, 默认值到这里才写进 SettingService, 之后才能读值
    settingsController->commitDefinitions();

    hideAfterPaste = settingsController->value(QStringLiteral("core/hideAfterPaste")).toBool();
    showTrayIcon = settingsController->value(QStringLiteral("core/showTrayIcon")).toBool();
    connect(settingsController, &SettingsController::settingValueChanged, this,
            [this](const QString& key, const QVariant& value) {
                if (key == QStringLiteral("core/hideAfterPaste")) {
                    hideAfterPaste = value.toBool();
                } else if (key == QStringLiteral("core/showTrayIcon")) {
                    showTrayIcon = value.toBool();
                    trayIcon.setVisible(showTrayIcon);
                }
            });

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

    applyTheme();
    setupGlobalHotkey();
    setupTray();
    setCentralWidget(&central);
    connect(&head, &CustomHead::closeRequested, this, &QWidget::close);
    connect(&head, &CustomHead::settingsRequested, this, &MainWindow::openSettings);
    connect(&head, &CustomHead::moveRequested, this, [=](QPoint pos) { move(pos); });
    setupUI();

    contentList.setItems(controller->getCopyDate());
    refreshTagBar();
    connect(controller, &UIController::updateUI, this, &MainWindow::updateCopyList);
    connect(&searchWidget, &SearchWidget::inputTextChanged, controller,
            &UIController::requireSearch);
    connect(&tagListView, &QListView::clicked, this, [this](const QModelIndex& index) {
        controller->requireTagFilter(index.data(TagBarNameRole).toString());
    });
    connect(&contentList, &ContentListWidget::itemClicked, controller, &UIController::pasteContent);
    connect(controller, &UIController::hideWindowRequested, this, [this] {
        if (hideAfterPaste) {
            hideWindow();
        }
    });
}

MainWindow::~MainWindow() {
    teardownGlobalHotkey();
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        applyTheme();
    }

    if (event->type() == QEvent::ActivationChange && isVisible() && !isActiveWindow() &&
        !trayExitRequested && !settingsDialogOpen) {
        hideWindow();
    }

    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (trayExitRequested || !trayIcon.isVisible()) {
        QMainWindow::closeEvent(event);
        return;
    }

    hideWindow();
    event->ignore();
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(eventType);
#ifdef Q_OS_WIN
    MSG* msg = reinterpret_cast<MSG*>(message);
    if (msg != nullptr && msg->message == WM_HOTKEY && msg->wParam == kHotkeyId) {
        showWindow();
        if (result != nullptr) {
            *result = 0;
        }
        return true;
    }
#else
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif

    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::showWindow() {
    resetTagFilter();

    // 窗口位置由平台实现决定: Windows 跟随插入符/鼠标, 其他平台落到屏幕右下角
    move(windowPositioner->resolvePosition(size()));

    if (isMinimized()) {
        showNormal();
    } else {
        show();
    }
    contentList.scrollToTop();

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd != nullptr) {
        SetForegroundWindow(hwnd);
    }
#endif

    raise();
    activateWindow();
    searchWidget.focusInput();
}

void MainWindow::hideWindow() {
    hide();
}

void MainWindow::openSettings() {
    settingsDialogOpen = true;
    // 对话框的创建、推值与信号串接都交给 SettingsController
    settingsController->openDialog(this);
    settingsDialogOpen = false;

    // hideAfterPaste / showTrayIcon 由 settingValueChanged 跟着值变化同步, 这里不用再读一次
    trayIcon.setVisible(showTrayIcon);

    // 标签设置可能已变更, 刷新主窗口标签栏与内容列表
    refreshTagBar();
    resetTagFilter();

    raise();
    activateWindow();
}

void MainWindow::exitFromTray() {
    trayExitRequested = true;
    teardownGlobalHotkey();
    trayIcon.hide();
    close();
    qApp->quit();
}

void MainWindow::updateCopyList(QVector<ContentListItemData> data) {
    contentList.setItems(data);
}
