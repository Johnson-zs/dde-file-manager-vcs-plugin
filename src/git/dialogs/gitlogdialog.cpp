#include "gitlogdialog.h"
#include "widgets/gitcommitdetailswidget.h"
#include "gitlogdatamanager.h"
#include "gitlogsearchmanager.h"
#include "gitlogcontextmenumanager.h"
#include "widgets/linenumbertextedit.h"
#include "widgets/searchablebranchselector.h"
#include "widgets/characteranimationwidget.h"
#include "gitdialogs.h"
#include "gitoperationdialog.h"
#include "gitfilepreviewdialog.h"

#include <QApplication>
#include <QMessageBox>
#include <QFont>
#include <QFileInfo>
#include <QMenu>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QProcess>
#include <QDir>
#include <QRegularExpression>
#include <QScreen>

GitLogDialog::GitLogDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
    : QDialog(parent)
{
    initializeDialog(repositoryPath, filePath, QString());
}

GitLogDialog::GitLogDialog(const QString &repositoryPath, const QString &filePath, const QString &initialBranch, QWidget *parent)
    : QDialog(parent)
{
    initializeDialog(repositoryPath, filePath, initialBranch);
}

GitLogDialog::~GitLogDialog()
{
    qInfo() << "INFO: [GitLogDialog] Destroying refactored GitLogDialog";
}

void GitLogDialog::initializeDialog(const QString &repositoryPath, const QString &filePath, const QString &initialBranch)
{
    m_repositoryPath = repositoryPath;
    m_filePath = filePath;
    m_initialBranch = initialBranch;
    m_isLoadingMore = false;
    m_enableChangeStats = true;
    m_currentPreviewDialog = nullptr;

    qInfo() << "INFO: [GitLogDialog] Initializing refactored GitLogDialog for repository:" << repositoryPath;

    // 创建核心组件（组合模式）
    m_dataManager = new GitLogDataManager(repositoryPath, this);
    m_commitDetailsWidget = new GitCommitDetailsWidget(this);
    m_contextMenuManager = new GitLogContextMenuManager(repositoryPath, this);
    m_searchManager = nullptr;   // 延迟创建

    setupUI();
    connectSignals();

    // 安装事件过滤器
    m_changedFilesTree->installEventFilter(this);

    // 修复：改进初始化加载逻辑，确保远程commits能在初始加载时显示
    QTimer::singleShot(0, this, [this]() {
        m_dataManager->setFilePath(m_filePath);
        m_dataManager->loadBranches();
        // 重要修复：不直接加载commits，等待分支加载完成后在onBranchesLoaded中处理
    });

    qInfo() << "INFO: [GitLogDialog] Refactored GitLogDialog initialized successfully";
}

void GitLogDialog::setupUI()
{
    // === 修改：窗口标题显示项目名称 ===
    QString repositoryName = getRepositoryName();
    QString windowTitle;
    if (m_filePath.isEmpty()) {
        windowTitle = tr("Git Log - %1").arg(repositoryName);
    } else {
        windowTitle = tr("Git Log - %1 (%2)").arg(QFileInfo(m_filePath).fileName(), repositoryName);
    }
    setWindowTitle(windowTitle);

    setModal(false);

    // === 新增：基于屏幕分辨率的自适应窗口尺寸 ===
    setupAdaptiveWindowSize();

    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    setupToolbar();
    setupMainLayout();

    mainLayout->addLayout(m_toolbarLayout);
    mainLayout->addWidget(m_loadingWidget);
    mainLayout->addWidget(m_mainSplitter);

    setupInfiniteScroll();
}

void GitLogDialog::setupAdaptiveWindowSize()
{
    // 获取主屏幕的可用几何区域（排除任务栏等）
    QScreen *primaryScreen = QApplication::primaryScreen();
    if (!primaryScreen) {
        // 回退到固定尺寸
        setMinimumSize(1000, 700);
        resize(1200, 800);
        return;
    }

    QRect availableGeometry = primaryScreen->availableGeometry();
    int screenWidth = availableGeometry.width();
    int screenHeight = availableGeometry.height();

    // 简单的尺寸计算：占屏幕的75%，但不超过合理的最大值
    int windowWidth = qMin(static_cast<int>(screenWidth * 0.75), 1400);
    int windowHeight = qMin(static_cast<int>(screenHeight * 0.75), 900);

    // 确保最小尺寸
    windowWidth = qMax(windowWidth, 1000);
    windowHeight = qMax(windowHeight, 700);

    setMinimumSize(1000, 700);
    resize(windowWidth, windowHeight);

    // 居中显示
    QRect windowGeometry = geometry();
    windowGeometry.moveCenter(availableGeometry.center());
    setGeometry(windowGeometry);
}

void GitLogDialog::setupToolbar()
{
    m_toolbarLayout = new QHBoxLayout;
    m_toolbarLayout->setSpacing(8);

    // 分支选择器
    m_toolbarLayout->addWidget(new QLabel(tr("Branch:")));
    m_branchSelector = new SearchableBranchSelector;
    m_branchSelector->setPlaceholderText(tr("Select branch or tag..."));
    m_branchSelector->setMinimumWidth(300);
    m_branchSelector->setToolTip(tr("Select branch or tag to view commit history"));
    m_toolbarLayout->addWidget(m_branchSelector);

    m_toolbarLayout->addSpacing(16);

    // 搜索框
    m_toolbarLayout->addWidget(new QLabel(tr("Search:")));
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("Search commits, authors, messages..."));
    m_searchEdit->setMinimumWidth(250);
    m_searchEdit->setToolTip(tr("Search in commit messages, authors, and hashes"));
    m_toolbarLayout->addWidget(m_searchEdit);

    // 搜索状态标签
    m_searchStatusLabel = new QLabel;
    m_searchStatusLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    m_searchStatusLabel->hide();
    m_toolbarLayout->addWidget(m_searchStatusLabel);

    m_toolbarLayout->addSpacing(16);

    // 操作按钮
    m_refreshButton = new QPushButton(tr("Refresh"));
    m_refreshButton->setIcon(QIcon::fromTheme("view-refresh"));
    m_refreshButton->setToolTip(tr("Refresh commit history"));
    m_toolbarLayout->addWidget(m_refreshButton);

    m_settingsButton = new QPushButton(tr("Settings"));
    m_settingsButton->setIcon(QIcon::fromTheme("configure"));
    m_settingsButton->setToolTip(tr("Configure log display options"));
    m_toolbarLayout->addWidget(m_settingsButton);

    m_toolbarLayout->addStretch();
}

void GitLogDialog::setupMainLayout()
{
    // 主分割器（水平）
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    setupCommitList();

    // === 新增：设置加载状态指示器 ===
    m_loadingWidget = new QWidget;
    m_loadingWidget->setFixedHeight(50);
    m_loadingWidget->setVisible(false);   // 初始状态隐藏

    auto *loadingLayout = new QHBoxLayout(m_loadingWidget);
    loadingLayout->setContentsMargins(16, 8, 16, 8);

    m_loadingAnimation = new CharacterAnimationWidget(m_loadingWidget);
    m_loadingAnimation->setTextStyleSheet("QLabel { color: #2196F3; font-weight: bold; font-size: 14px; }");

    loadingLayout->addWidget(m_loadingAnimation);
    loadingLayout->addStretch();

    // 右侧分割器（垂直）
    m_rightSplitter = new QSplitter(Qt::Vertical);

    // 使用重构的提交详情组件
    m_rightSplitter->addWidget(m_commitDetailsWidget);

    setupFilesList();
    setupDiffView();

    m_rightSplitter->addWidget(m_changedFilesTree);
    m_rightSplitter->addWidget(m_diffView);

    // 设置比例：详情30%，文件列表20%，差异50%
    m_rightSplitter->setSizes({ 300, 200, 500 });
    m_rightSplitter->setStretchFactor(0, 1);
    m_rightSplitter->setStretchFactor(1, 1);
    m_rightSplitter->setStretchFactor(2, 2);

    // 设置分割器比例：左侧40%，右侧60%
    m_mainSplitter->addWidget(m_commitTree);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setSizes({ 400, 600 });
    m_mainSplitter->setStretchFactor(0, 2);
    m_mainSplitter->setStretchFactor(1, 3);
}

void GitLogDialog::setupCommitList()
{
    m_commitTree = new QTreeWidget;
    m_commitTree->setHeaderLabels({ tr("Graph"), tr("Message"), tr("Author"), tr("Date"), tr("Hash") });
    m_commitTree->setRootIsDecorated(false);
    m_commitTree->setAlternatingRowColors(true);
    m_commitTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_commitTree->setContextMenuPolicy(Qt::CustomContextMenu);

    // 设置列宽
    m_commitTree->setColumnWidth(0, 60);   // Graph
    m_commitTree->setColumnWidth(1, 300);   // Message
    m_commitTree->setColumnWidth(2, 120);   // Author
    m_commitTree->setColumnWidth(3, 120);   // Date
    m_commitTree->setColumnWidth(4, 80);   // Hash

    // 设置字体
    QFont commitFont("Consolas", 9);
    m_commitTree->setFont(commitFont);
}

void GitLogDialog::setupFilesList()
{
    m_changedFilesTree = new QTreeWidget;
    m_changedFilesTree->setHeaderLabels({ tr("Status"), tr("File"), tr("Changes") });
    m_changedFilesTree->setRootIsDecorated(false);
    m_changedFilesTree->setAlternatingRowColors(true);
    m_changedFilesTree->setContextMenuPolicy(Qt::CustomContextMenu);

    // 设置列宽
    m_changedFilesTree->setColumnWidth(0, 60);   // Status
    m_changedFilesTree->setColumnWidth(1, 300);   // File
    m_changedFilesTree->setColumnWidth(2, 100);   // Changes
}

void GitLogDialog::setupDiffView()
{
    m_diffView = new LineNumberTextEdit;
    m_diffView->setReadOnly(true);
    m_diffView->setFont(QFont("Consolas", 9));
    m_diffView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_diffView->setPlainText(tr("Select a file to view changes..."));

    // 设置语法高亮
    m_diffHighlighter = new GitDiffSyntaxHighlighter(m_diffView->document());
}

void GitLogDialog::setupInfiniteScroll()
{
    m_commitScrollBar = m_commitTree->verticalScrollBar();

    m_loadTimer = new QTimer(this);
    m_loadTimer->setSingleShot(true);
    m_loadTimer->setInterval(300);   // 300ms延迟
}

void GitLogDialog::connectSignals()
{
    // 工具栏信号
    connect(m_branchSelector, &SearchableBranchSelector::selectionChanged,
            this, &GitLogDialog::onBranchSelectorChanged);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &GitLogDialog::onSearchTextChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &GitLogDialog::onRefreshClicked);
    connect(m_settingsButton, &QPushButton::clicked,
            this, &GitLogDialog::onSettingsClicked);

    // 提交列表信号
    connect(m_commitTree, &QTreeWidget::itemSelectionChanged,
            this, &GitLogDialog::onCommitSelectionChanged);
    connect(m_commitTree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
                QTreeWidgetItem *item = m_commitTree->itemAt(pos);
                if (item) {
                    m_commitTree->setCurrentItem(item);
                    QString commitHash = getCurrentSelectedCommitHash();
                    QString commitMessage = item->text(1);

                    // === 修复：获取commit来源信息以正确控制浏览器打开选项 ===
                    bool isRemoteCommit = false;
                    bool hasRemoteUrl = false;

                    // 检查是否有远程URL
                    QString remoteUrl = getRemoteUrl("origin");
                    hasRemoteUrl = !remoteUrl.isEmpty();

                    // 从数据管理器获取commit信息来判断是否为远程commit
                    const auto &commits = m_dataManager->getCommits();
                    for (const auto &commit : commits) {
                        if (commit.fullHash == commitHash) {
                            // 只有远程专有的commit或者已同步到远程的commit才显示浏览器打开选项
                            isRemoteCommit = (commit.source == GitLogDataManager::CommitSource::Remote || commit.source == GitLogDataManager::CommitSource::Both);
                            break;
                        }
                    }

                    // 使用新的重载方法，传递commit来源信息
                    m_contextMenuManager->showCommitContextMenu(
                            m_commitTree->mapToGlobal(pos), commitHash, commitMessage,
                            isRemoteCommit, hasRemoteUrl);
                }
            });

    // 文件列表信号
    connect(m_changedFilesTree, &QTreeWidget::itemSelectionChanged,
            this, &GitLogDialog::onFileSelectionChanged);
    connect(m_changedFilesTree, &QTreeWidget::itemDoubleClicked,
            this, &GitLogDialog::onFileDoubleClicked);
    connect(m_changedFilesTree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
                QTreeWidgetItem *item = m_changedFilesTree->itemAt(pos);
                if (item) {
                    m_changedFilesTree->setCurrentItem(item);
                    QString commitHash = getCurrentSelectedCommitHash();
                    QString filePath = getCurrentSelectedFilePath();
                    m_contextMenuManager->showFileContextMenu(
                            m_changedFilesTree->mapToGlobal(pos), commitHash, filePath);
                }
            });

    // 滚动信号
    connect(m_commitScrollBar, &QScrollBar::valueChanged,
            this, &GitLogDialog::onScrollValueChanged);
    connect(m_loadTimer, &QTimer::timeout,
            this, &GitLogDialog::loadMoreCommitsIfNeeded);

    // 数据管理器信号
    connect(m_dataManager, &GitLogDataManager::commitHistoryLoaded,
            this, &GitLogDialog::onCommitHistoryLoaded);
    connect(m_dataManager, &GitLogDataManager::branchesLoaded,
            this, &GitLogDialog::onBranchesLoaded);
    connect(m_dataManager, &GitLogDataManager::commitDetailsLoaded,
            this, &GitLogDialog::onCommitDetailsLoaded);
    connect(m_dataManager, &GitLogDataManager::commitFilesLoaded,
            this, &GitLogDialog::onCommitFilesLoaded);
    connect(m_dataManager, &GitLogDataManager::fileStatsLoaded,
            this, &GitLogDialog::onFileStatsLoaded);
    connect(m_dataManager, &GitLogDataManager::fileDiffLoaded,
            this, &GitLogDialog::onFileDiffLoaded);
    connect(m_dataManager, &GitLogDataManager::dataLoadError,
            this, &GitLogDialog::onDataLoadError);
    connect(m_dataManager, &GitLogDataManager::remoteStatusUpdated,
            this, &GitLogDialog::onRemoteStatusUpdated);
    connect(m_dataManager, &GitLogDataManager::remoteReferencesUpdated,
            this, &GitLogDialog::onRemoteReferencesUpdated);

    // 右键菜单管理器信号
    connect(m_contextMenuManager, &GitLogContextMenuManager::gitOperationRequested,
            this, &GitLogDialog::onGitOperationRequested);
    connect(m_contextMenuManager, &GitLogContextMenuManager::compareWithWorkingTreeRequested,
            this, &GitLogDialog::onCompareWithWorkingTreeRequested);
    connect(m_contextMenuManager, &GitLogContextMenuManager::viewFileAtCommitRequested,
            this, &GitLogDialog::onViewFileAtCommitRequested);
    connect(m_contextMenuManager, &GitLogContextMenuManager::showFileDiffRequested,
            this, &GitLogDialog::onShowFileDiffRequested);
    connect(m_contextMenuManager, &GitLogContextMenuManager::showFileHistoryRequested,
            this, &GitLogDialog::onShowFileHistoryRequested);
    connect(m_contextMenuManager, &GitLogContextMenuManager::showFileBlameRequested,
            this, &GitLogDialog::onShowFileBlameRequested);
    connect(m_contextMenuManager, &GitLogContextMenuManager::refreshRequested,
            this, &GitLogDialog::onRefreshClicked);
    connect(m_contextMenuManager, &GitLogContextMenuManager::openFileRequested,
            this, &GitLogDialog::onOpenFileRequested);
    connect(m_contextMenuManager, &GitLogContextMenuManager::showFileInFolderRequested,
            this, &GitLogDialog::onShowFileInFolderRequested);

    // === 新增：浏览器打开commit信号连接 ===
    connect(m_contextMenuManager, &GitLogContextMenuManager::openCommitInBrowserRequested,
            this, &GitLogDialog::onOpenCommitInBrowserRequested);

    // 提交详情组件信号
    connect(m_commitDetailsWidget, &GitCommitDetailsWidget::linkClicked,
            this, [this](const QString &url) {
                qDebug() << "[GitLogDialog] Commit details link clicked:" << url;
                if (url.startsWith("http://") || url.startsWith("https://")) {
                    QDesktopServices::openUrl(QUrl(url));
                }
            });
}

// === 槽函数实现 ===

void GitLogDialog::onCommitSelectionChanged()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) {
        return;
    }

    // 委托给数据管理器加载数据
    m_dataManager->loadCommitDetails(commitHash);
    m_dataManager->loadCommitFiles(commitHash);
}

void GitLogDialog::onFileSelectionChanged()
{
    QString commitHash = getCurrentSelectedCommitHash();
    QString filePath = getCurrentSelectedFilePath();

    if (!commitHash.isEmpty() && !filePath.isEmpty()) {
        m_dataManager->loadFileDiff(commitHash, filePath);
    } else {
        m_diffView->setPlainText(tr("Select a file to view changes..."));
    }
}

void GitLogDialog::onFileDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (item) {
        QString commitHash = getCurrentSelectedCommitHash();
        QString filePath = getCurrentSelectedFilePath();
        if (!commitHash.isEmpty() && !filePath.isEmpty()) {
            onShowFileDiffRequested(commitHash, filePath);
        }
    }
}

void GitLogDialog::onRefreshClicked()
{
    qInfo() << "INFO: [GitLogDialog] Refreshing commit history";
    m_dataManager->clearCache();
    m_dataManager->loadBranches();

    QString currentBranch = m_branchSelector->getCurrentSelection();

    // === 修改：刷新时强制更新远程引用（使用全局策略） ===
    if (!currentBranch.isEmpty() && currentBranch != "HEAD") {
        qInfo() << "INFO: [GitLogDialog] Force updating all remote references during refresh";

        // === 新增：显示刷新状态 ===
        showLoadingStatus(tr("Refreshing remote data..."));

        // 清除远程引用缓存，强制更新（现在是全局的）
        m_dataManager->clearRemoteRefTimestampCache();

        // 连接更新完成信号，在完成后隐藏加载状态
        connect(
                m_dataManager, &GitLogDataManager::remoteReferencesUpdated,
                this, [this](const QString &branch, bool success) {
                    Q_UNUSED(branch)
                    Q_UNUSED(success)

                    // 断开这个临时连接
                    disconnect(m_dataManager, &GitLogDataManager::remoteReferencesUpdated,
                               this, nullptr);

                    // === 新增：隐藏加载状态 ===
                    hideLoadingStatus();
                },
                Qt::SingleShotConnection);

        // 异步更新所有远程引用（不阻塞UI）
        m_dataManager->updateRemoteReferencesAsync(currentBranch);
    }

    m_dataManager->loadCommitHistory(currentBranch);

    // 直接刷新远程状态
    if (!currentBranch.isEmpty() && currentBranch != "HEAD") {
        qInfo() << "INFO: [GitLogDialog] Refreshing remote status for:" << currentBranch;
        m_dataManager->updateCommitRemoteStatus(currentBranch);
    }
}

void GitLogDialog::onSettingsClicked()
{
    QMenu settingsMenu(this);

    // 改动统计选项
    QAction *changeStatsAction = settingsMenu.addAction(tr("Enable Change Statistics"));
    changeStatsAction->setCheckable(true);
    changeStatsAction->setChecked(m_enableChangeStats);
    changeStatsAction->setToolTip(tr("Show/hide file change statistics (+/-) in the file list"));

    connect(changeStatsAction, &QAction::triggered, this, [this](bool enabled) {
        m_enableChangeStats = enabled;
        qInfo() << "INFO: [GitLogDialog] Change statistics" << (enabled ? "enabled" : "disabled");

        // 如果启用了统计，重新加载当前commit的统计信息
        if (enabled) {
            QString currentCommit = getCurrentSelectedCommitHash();
            if (!currentCommit.isEmpty()) {
                m_dataManager->loadFileChangeStats(currentCommit);
            }
        }
    });

    settingsMenu.addSeparator();

    // 分支选择器设置
    QAction *showRemoteBranchesAction = settingsMenu.addAction(tr("Show Remote Branches"));
    showRemoteBranchesAction->setCheckable(true);
    showRemoteBranchesAction->setChecked(m_branchSelector->getShowRemoteBranches());
    connect(showRemoteBranchesAction, &QAction::triggered, this, [this](bool show) {
        m_branchSelector->setShowRemoteBranches(show);
        qInfo() << "INFO: [GitLogDialog] Remote branches" << (show ? "shown" : "hidden");
    });

    QAction *showTagsAction = settingsMenu.addAction(tr("Show Tags"));
    showTagsAction->setCheckable(true);
    showTagsAction->setChecked(m_branchSelector->getShowTags());
    connect(showTagsAction, &QAction::triggered, this, [this](bool show) {
        m_branchSelector->setShowTags(show);
        qInfo() << "INFO: [GitLogDialog] Tags" << (show ? "shown" : "hidden");
    });

    settingsMenu.addSeparator();

    QAction *aboutAction = settingsMenu.addAction(tr("About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, tr("About Git Log Dialog"),
                                 tr("Refactored Git Log Dialog\n\n"
                                    "Features:\n"
                                    "• Modular architecture with specialized components\n"
                                    "• GitCommitDetailsWidget for reusable commit details\n"
                                    "• GitLogDataManager for data management and caching\n"
                                    "• GitLogSearchManager for search functionality\n"
                                    "• GitLogContextMenuManager for menu operations\n"
                                    "• Adaptive window sizing for different screen resolutions\n"
                                    "• Improved maintainability and testability"));
    });

    settingsMenu.exec(m_settingsButton->mapToGlobal(QPoint(0, m_settingsButton->height())));
}

void GitLogDialog::onBranchSelectorChanged(const QString &branchName)
{
    qInfo() << "INFO: [GitLogDialog] Branch selector changed to:" << branchName;

    // 跳过"All Branches"选项，因为它没有实际用途
    if (branchName == tr("All Branches")) {
        qInfo() << "INFO: [GitLogDialog] Skipping 'All Branches' selection - not implemented";
        return;
    }

    m_dataManager->clearCommitCache();

    // 根据分支类型采用不同的加载策略
    if (!branchName.isEmpty() && branchName != "HEAD") {
        qInfo() << "INFO: [GitLogDialog] Loading commits for branch:" << branchName;

        // 先加载远程跟踪信息
        m_dataManager->loadAllRemoteTrackingInfo(branchName);

        // 检查是否需要加载远程commits
        if (m_dataManager->shouldLoadRemoteCommits(branchName)) {
            qInfo() << "INFO: [GitLogDialog] Branch has remote tracking, loading with remote commits";
            m_dataManager->loadCommitHistoryWithRemote(branchName);
        } else {
            qInfo() << "INFO: [GitLogDialog] Loading local commits only";
            m_dataManager->loadCommitHistory(branchName);
        }

        // 延迟更新远程状态
        QTimer::singleShot(200, this, [this, branchName]() {
            if (m_dataManager) {
                qInfo() << "INFO: [GitLogDialog] Updating remote status for:" << branchName;
                m_dataManager->updateCommitRemoteStatus(branchName);
            }
        });
    } else {
        // 加载默认的commits (当前HEAD)
        qInfo() << "INFO: [GitLogDialog] Loading default commits";
        m_dataManager->loadCommitHistory();
    }
}

void GitLogDialog::onSearchTextChanged()
{
    if (m_searchManager) {
        m_searchManager->startSearch(m_searchEdit->text());
    }
}

void GitLogDialog::onScrollValueChanged(int value)
{
    int maximum = m_commitScrollBar->maximum();
    if (maximum > 0 && value >= maximum - PRELOAD_THRESHOLD) {
        qDebug() << "[GitLogDialog] Scroll near bottom, triggering load more. Value:" << value << "Maximum:" << maximum;
        m_loadTimer->start();
    }
}

// === 数据管理器信号响应 ===

void GitLogDialog::onCommitHistoryLoaded(const QList<GitLogDataManager::CommitInfo> &commits, bool append)
{
    // 在append模式时，保存当前滚动位置和选中项的hash
    int savedScrollPosition = -1;
    QString selectedCommitHash;

    if (append && m_commitTree && m_commitScrollBar) {
        savedScrollPosition = m_commitScrollBar->value();

        // 保存当前选中项的commit hash而不是指针，避免悬垂指针
        QTreeWidgetItem *currentItem = m_commitTree->currentItem();
        if (currentItem) {
            selectedCommitHash = currentItem->data(4, Qt::UserRole).toString();
        }

        qDebug() << "DEBUG: [GitLogDialog] Saving scroll position:" << savedScrollPosition
                 << "selected commit:" << selectedCommitHash.left(8);
    }

    populateCommitList(commits, append);

    // 创建搜索管理器（延迟创建）
    if (!m_searchManager) {
        m_searchManager = new GitLogSearchManager(m_commitTree, m_searchStatusLabel, this);
        connect(m_searchManager, &GitLogSearchManager::moreDataNeeded,
                this, &GitLogDialog::onMoreDataNeeded);
    } else {
        // 通知搜索管理器新数据已加载
        m_searchManager->onNewCommitsLoaded();
    }

    // 修复：只在首次加载时选中第一个本地commit，append模式时保持原有选中状态
    if (!append && m_commitTree->topLevelItemCount() > 0) {
        selectFirstLocalCommit();
        qInfo() << "INFO: [GitLogDialog] Auto-selected first commit after initial loading";
    } else if (append && savedScrollPosition >= 0) {
        // 在append模式时，恢复滚动位置和选中状态
        // 使用QTimer延迟恢复，但增加安全检查
        QTimer::singleShot(50, this, [this, savedScrollPosition, selectedCommitHash]() {
            // 安全检查：确保对象仍然有效
            if (!this || !m_commitTree || !m_commitScrollBar) {
                qWarning() << "WARNING: [GitLogDialog] Object destroyed during delayed scroll restore";
                return;
            }

            // 恢复滚动位置
            m_commitScrollBar->setValue(savedScrollPosition);
            qDebug() << "DEBUG: [GitLogDialog] Restored scroll position:" << savedScrollPosition;

            // 恢复选中项（根据commit hash查找）
            if (!selectedCommitHash.isEmpty()) {
                for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
                    QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
                    if (item && item->data(4, Qt::UserRole).toString() == selectedCommitHash) {
                        m_commitTree->setCurrentItem(item);
                        qDebug() << "DEBUG: [GitLogDialog] Restored selected commit:" << selectedCommitHash.left(8);
                        break;
                    }
                }
            }
        });
        qInfo() << "INFO: [GitLogDialog] Appended commits, will restore scroll position";
    }

    m_isLoadingMore = false;
}

void GitLogDialog::onBranchesLoaded(const GitLogDataManager::BranchInfo &branchInfo)
{
    m_branchSelector->setBranches(branchInfo.localBranches, branchInfo.remoteBranches,
                                  branchInfo.tags, branchInfo.currentBranch);

    // 设置初始分支
    QString initialBranch;
    if (!m_initialBranch.isEmpty()) {
        initialBranch = m_initialBranch;
        m_branchSelector->setCurrentSelection(m_initialBranch);
    } else if (!branchInfo.currentBranch.isEmpty()) {
        initialBranch = branchInfo.currentBranch;
        m_branchSelector->setCurrentSelection(branchInfo.currentBranch);
    }

    // === 新增：在初始化时主动检查并更新远程引用 ===
    if (!initialBranch.isEmpty() && initialBranch != "HEAD") {
        qInfo() << "INFO: [GitLogDialog] Branches loaded, checking remote references for initial branch:" << initialBranch;

        // 检查是否需要更新远程引用
        if (m_dataManager->shouldUpdateRemoteReferences(initialBranch)) {
            qInfo() << "INFO: [GitLogDialog] Remote references are outdated, updating before loading commits";

            // === 新增：显示加载状态 ===
            showLoadingStatus(tr("Fetching remote updates..."));

            // 连接远程引用更新完成信号，确保更新完成后再加载commits
            connect(
                    m_dataManager, &GitLogDataManager::remoteReferencesUpdated,
                    this, [this, initialBranch](const QString &branch, bool success) {
                        Q_UNUSED(branch)

                        // 断开这个临时连接，避免重复执行
                        disconnect(m_dataManager, &GitLogDataManager::remoteReferencesUpdated,
                                   this, nullptr);

                        // === 新增：隐藏加载状态 ===
                        hideLoadingStatus();

                        if (success) {
                            qInfo() << "INFO: [GitLogDialog] Remote references updated successfully, now loading commits";
                        } else {
                            qWarning() << "WARNING: [GitLogDialog] Remote references update failed, loading with existing data";
                        }

                        // 现在加载commits（无论更新成功还是失败）
                        loadCommitsForInitialBranch(initialBranch);
                    },
                    Qt::SingleShotConnection);

            // 异步更新远程引用
            m_dataManager->updateRemoteReferencesAsync(initialBranch);
        } else {
            qInfo() << "INFO: [GitLogDialog] Remote references are up to date, loading commits directly";
            // 远程引用是最新的，直接加载commits
            loadCommitsForInitialBranch(initialBranch);
        }
    } else {
        // 如果没有有效的初始分支，加载默认的commits
        qInfo() << "INFO: [GitLogDialog] No valid initial branch, loading default commits";
        m_dataManager->loadCommitHistory();
    }
}

void GitLogDialog::onCommitDetailsLoaded(const QString &commitHash, const QString &details)
{
    // 委托给提交详情组件显示
    m_commitDetailsWidget->setCommitDetails(details);
}

void GitLogDialog::onCommitFilesLoaded(const QString &commitHash, const QList<GitLogDataManager::FileChangeInfo> &files)
{
    populateFilesList(files);

    // 如果启用了统计功能，异步加载统计信息
    if (m_enableChangeStats) {
        m_dataManager->loadFileChangeStats(commitHash);
    }
}

void GitLogDialog::onFileStatsLoaded(const QString &commitHash, const QList<GitLogDataManager::FileChangeInfo> &files)
{
    // 更新文件列表的统计信息并计算总计
    populateFilesList(files);

    // 计算总的统计信息并更新提交详情
    int totalAdditions = 0;
    int totalDeletions = 0;
    int filesChanged = files.size();

    for (const auto &file : files) {
        if (file.statsLoaded) {
            totalAdditions += file.additions;
            totalDeletions += file.deletions;
        }
    }

    // 委托给提交详情组件更新统计信息
    m_commitDetailsWidget->setCommitSummaryStats(filesChanged, totalAdditions, totalDeletions);
}

void GitLogDialog::onFileDiffLoaded(const QString &commitHash, const QString &filePath, const QString &diff)
{
    Q_UNUSED(commitHash)
    Q_UNUSED(filePath)
    m_diffView->setPlainText(diff);
}

void GitLogDialog::onDataLoadError(const QString &operation, const QString &error)
{
    QMessageBox::warning(this, tr("Git Operation Error"),
                         tr("Failed to %1:\n%2").arg(operation, error));
}

void GitLogDialog::onRemoteStatusUpdated(const QString &branch)
{
    if (!m_dataManager) {
        qCritical() << "CRITICAL: [GitLogDialog::onRemoteStatusUpdated] m_dataManager is null";
        return;
    }

    qInfo() << "INFO: [GitLogDialog] Received remote status update for branch:" << branch;

    // 重新渲染commit列表以显示更新的远程状态
    const auto &commits = m_dataManager->getCommits();

    if (commits.isEmpty()) {
        qWarning() << "WARNING: [GitLogDialog] No commits to update remote status for";
        return;
    }

    populateCommitList(commits, false);

    // 修复：远程状态更新后，如果没有选中项，选中第一个本地commit
    if (m_commitTree->topLevelItemCount() > 0 && !m_commitTree->currentItem()) {
        selectFirstLocalCommit();
        qInfo() << "INFO: [GitLogDialog] Auto-selected first commit after remote status update";
    }

    qInfo() << QString("INFO: [GitLogDialog] Remote status updated for branch: %1, refreshed %2 commits display")
                       .arg(branch)
                       .arg(commits.size());
}

void GitLogDialog::onRemoteReferencesUpdated(const QString &branch, bool success)
{
    if (success) {
        qInfo() << QString("INFO: [GitLogDialog] Remote references updated successfully for branch: %1").arg(branch);

        // 远程引用更新成功后，重新加载commit历史以获取最新的远程状态
        QString currentBranch = m_branchSelector->getCurrentSelection();
        if (currentBranch == branch) {
            qInfo() << "INFO: [GitLogDialog] Refreshing commit history after remote update";

            // 清除缓存并重新加载
            m_dataManager->clearCommitCache();

            // 重新加载commit历史（包括远程commits）
            if (m_dataManager->shouldLoadRemoteCommits(branch)) {
                m_dataManager->loadCommitHistoryWithRemote(branch);
            } else {
                m_dataManager->loadCommitHistory(branch);
            }
        }
    } else {
        qWarning() << QString("WARNING: [GitLogDialog] Failed to update remote references for branch: %1").arg(branch);
        // 即使更新失败，也继续使用现有数据，不阻断用户操作
    }
}

// === 搜索管理器信号响应 ===

void GitLogDialog::onSearchStarted(const QString &searchText)
{
    Q_UNUSED(searchText)
    // 可以在这里添加搜索开始的处理逻辑
}

void GitLogDialog::onSearchCompleted(const QString &searchText, int totalResults)
{
    Q_UNUSED(searchText)
    Q_UNUSED(totalResults)
    // 可以在这里添加搜索完成的处理逻辑
}

void GitLogDialog::onSearchCleared()
{
    // 可以在这里添加搜索清除的处理逻辑
}

void GitLogDialog::onMoreDataNeeded()
{
    loadMoreCommitsIfNeeded();
}

// === 右键菜单信号响应 ===

void GitLogDialog::onGitOperationRequested(const QString &operation, const QStringList &args, bool needsConfirmation)
{
    if (needsConfirmation) {
        int ret = QMessageBox::question(this, tr("Confirm Operation"),
                                        tr("Are you sure you want to perform: %1?").arg(operation),
                                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (ret != QMessageBox::Yes) {
            return;
        }
    }

    // 使用GitOperationDialog执行Git命令
    auto *dialog = new GitOperationDialog(operation, this);
    dialog->executeCommand(m_repositoryPath, args);
    dialog->show();

    // 操作完成后刷新界面
    connect(dialog, &GitOperationDialog::finished, this, &GitLogDialog::refreshAfterOperation);

    qInfo() << QString("INFO: [GitLogDialog] Executing Git operation: %1 with args: %2")
                       .arg(operation, args.join(" "));
}

void GitLogDialog::onCompareWithWorkingTreeRequested(const QString &commitHash)
{
    if (commitHash.isEmpty()) {
        qWarning() << "WARNING: [GitLogDialog] Empty commit hash for compare with working tree";
        return;
    }

    qInfo() << "INFO: [GitLogDialog] Comparing commit" << commitHash.left(8) << "with working tree";

    // 复用GitBranchComparisonDialog来比较提交与工作树
    // 使用 commitHash 作为基础分支，"HEAD" 作为比较分支
    GitDialogManager::instance()->showBranchComparisonDialog(
            m_repositoryPath, commitHash, "HEAD", this);
}

void GitLogDialog::onShowFileDiffRequested(const QString &commitHash, const QString &filePath)
{
    if (commitHash.isEmpty() || filePath.isEmpty()) {
        qWarning() << "WARNING: [GitLogDialog] Empty commit hash or file path for diff request";
        return;
    }

    qInfo() << "INFO: [GitLogDialog] Showing file diff for:" << filePath
            << "at commit:" << commitHash.left(8);

    // === 修复：使用GitDialogManager的commit文件差异接口，复用GitDiffDialog ===
    GitDialogManager::instance()->showCommitFileDiffDialog(m_repositoryPath, filePath, commitHash, this);
}

void GitLogDialog::onViewFileAtCommitRequested(const QString &commitHash, const QString &filePath)
{
    GitDialogManager::instance()->showFilePreviewAtCommit(m_repositoryPath, filePath, commitHash, this);
}

void GitLogDialog::onShowFileHistoryRequested(const QString &filePath)
{
    // 显示指定文件的Git历史记录
    GitDialogManager::instance()->showLogDialog(m_repositoryPath, filePath, this);
    qInfo() << "INFO: [GitLogDialog] Opened file history for:" << filePath;
}

void GitLogDialog::onShowFileBlameRequested(const QString &filePath)
{
    // 显示指定文件的Git blame信息
    GitDialogManager::instance()->showBlameDialog(m_repositoryPath, filePath, this);
    qInfo() << "INFO: [GitLogDialog] Opened file blame for:" << filePath;
}

void GitLogDialog::onOpenFileRequested(const QString &filePath)
{
    GitDialogManager::instance()->openFile(filePath, this);
}

void GitLogDialog::onShowFileInFolderRequested(const QString &filePath)
{
    GitDialogManager::instance()->showFileInFolder(filePath, this);
}

// === 辅助方法 ===

QString GitLogDialog::getCurrentSelectedCommitHash() const
{
    auto selectedItems = m_commitTree->selectedItems();
    if (selectedItems.isEmpty()) {
        return QString();
    }
    return selectedItems.first()->data(4, Qt::UserRole).toString();
}

QString GitLogDialog::getCurrentSelectedFilePath() const
{
    auto selectedItems = m_changedFilesTree->selectedItems();
    if (selectedItems.isEmpty()) {
        return QString();
    }
    return selectedItems.first()->data(1, Qt::UserRole).toString();
}

void GitLogDialog::populateCommitList(const QList<GitLogDataManager::CommitInfo> &commits, bool append)
{
    if (!m_commitTree) {
        qCritical() << "CRITICAL: [GitLogDialog::populateCommitList] m_commitTree is null";
        return;
    }

    if (!append) {
        m_commitTree->clear();
    }

    qInfo() << QString("INFO: [GitLogDialog] Populating commit list with %1 commits (append: %2)")
                       .arg(commits.size())
                       .arg(append);

    int remoteStatusCount = 0;
    int remoteOnlyCount = 0;
    for (const auto &commit : commits) {
        auto *item = new QTreeWidgetItem(m_commitTree);

        // Graph列：显示图表信息和远程状态
        QString graphText = commit.graphInfo;

        // 添加远程状态指示器
        if (commit.remoteStatus != GitLogDataManager::RemoteStatus::Unknown) {
            QString statusText = getRemoteStatusText(commit.remoteStatus);
            graphText = QString("%1 %2").arg(statusText, commit.graphInfo);

            // 设置远程状态颜色
            QColor statusColor = getRemoteStatusColor(commit.remoteStatus);
            item->setForeground(0, QBrush(statusColor));

            // 设置工具提示
            QString tooltip = getRemoteStatusTooltip(commit.remoteStatus, commit.remoteRef);
            item->setToolTip(0, tooltip);

            remoteStatusCount++;
        } else {
            item->setToolTip(0, commit.graphInfo == "●" ? "Commit" : commit.graphInfo);
        }

        item->setText(0, graphText);
        item->setText(1, commit.message);
        item->setText(2, commit.author);
        item->setText(3, commit.date);
        item->setText(4, commit.shortHash);
        item->setData(4, Qt::UserRole, commit.fullHash);

        // 重要新增：根据commit来源设置不同的显示样式
        QColor baseColor = getCommitSourceColor(commit.source);

        // 设置行的颜色以区分本地和远程commits
        if (commit.source == GitLogDataManager::CommitSource::Remote) {
            // 远程专有commits使用紫色背景（类似VSCode）
            item->setBackground(0, QBrush(QColor(138, 43, 226, 30)));   // 紫色半透明背景
            item->setBackground(1, QBrush(QColor(138, 43, 226, 30)));
            item->setBackground(2, QBrush(QColor(138, 43, 226, 30)));
            item->setBackground(3, QBrush(QColor(138, 43, 226, 30)));
            item->setBackground(4, QBrush(QColor(138, 43, 226, 30)));

            // 设置紫色文字
            item->setForeground(1, QBrush(QColor(138, 43, 226)));   // 紫色文字
            item->setForeground(2, QBrush(QColor(138, 43, 226)));
            item->setForeground(3, QBrush(QColor(138, 43, 226)));
            item->setForeground(4, QBrush(QColor(138, 43, 226)));

            remoteOnlyCount++;
        } else if (commit.source == GitLogDataManager::CommitSource::Both) {
            // 本地和远程都有的commits使用默认样式但添加标记
            // 可以考虑添加特殊图标或边框
        }

        // 增强工具提示，显示分支信息
        QString enhancedTooltip = QString("Commit: %1\nMessage: %2\nBranches: %3")
                                          .arg(commit.fullHash.left(8))
                                          .arg(commit.message)
                                          .arg(commit.branches.join(", "));

        if (commit.source == GitLogDataManager::CommitSource::Remote) {
            enhancedTooltip += QString("\n[Remote Only] - Only exists on remote branch");
        } else if (commit.source == GitLogDataManager::CommitSource::Local) {
            enhancedTooltip += QString("\n[Local Only] - Only exists locally");
        } else {
            enhancedTooltip += QString("\n[Both] - Exists on both local and remote");
        }

        // 设置其他列的工具提示
        item->setToolTip(1, enhancedTooltip);
        item->setToolTip(2, QString("Author: %1").arg(commit.author));
        item->setToolTip(3, QString("Date: %1").arg(commit.date));
        item->setToolTip(4, QString("Full Hash: %1").arg(commit.fullHash));
    }

    qInfo() << QString("INFO: [GitLogDialog] Populated %1 commits, %2 have remote status, %3 are remote-only")
                       .arg(commits.size())
                       .arg(remoteStatusCount)
                       .arg(remoteOnlyCount);
}

void GitLogDialog::populateFilesList(const QList<GitLogDataManager::FileChangeInfo> &files)
{
    m_changedFilesTree->clear();

    for (const auto &file : files) {
        auto *item = new QTreeWidgetItem(m_changedFilesTree);

        // Status列
        item->setText(0, file.status);
        item->setIcon(0, getFileStatusIcon(file.status));

        // File列
        item->setText(1, QFileInfo(file.filePath).fileName());
        item->setData(1, Qt::UserRole, file.filePath);
        item->setToolTip(1, file.filePath);

        // Changes列
        if (file.statsLoaded) {
            QString statsText = formatChangeStats(file.additions, file.deletions);
            item->setText(2, statsText);
            setChangeStatsColor(item, file.additions, file.deletions);
        } else if (m_enableChangeStats) {
            item->setText(2, tr("Loading..."));
        } else {
            item->setText(2, tr("Disabled"));
            item->setForeground(2, QBrush(QColor(128, 128, 128)));
        }
    }
}

void GitLogDialog::previewSelectedFile()
{
    QString commitHash = getCurrentSelectedCommitHash();
    QString filePath = getCurrentSelectedFilePath();

    if (commitHash.isEmpty() || filePath.isEmpty()) {
        QMessageBox::information(this, tr("No File Selected"),
                                 tr("Please select a file to preview."));
        return;
    }

    // 关闭之前的预览对话框
    if (m_currentPreviewDialog) {
        m_currentPreviewDialog->close();
        m_currentPreviewDialog = nullptr;
    }

    // 创建新的预览对话框
    m_currentPreviewDialog = GitDialogManager::instance()->showFilePreviewAtCommit(
            m_repositoryPath, filePath, commitHash, this);

    // 连接对话框关闭信号
    if (m_currentPreviewDialog) {
        connect(m_currentPreviewDialog, &QDialog::finished, this, [this]() {
            m_currentPreviewDialog = nullptr;
        });
    }

    qInfo() << "INFO: [GitLogDialog] Opened file preview for:" << filePath
            << "at commit:" << commitHash.left(8);
}

void GitLogDialog::loadMoreCommitsIfNeeded()
{
    if (!m_isLoadingMore && m_dataManager->hasMoreCommits()) {
        m_isLoadingMore = true;
        int currentCount = m_dataManager->getTotalCommitsLoaded();
        QString currentBranch = m_branchSelector->getCurrentSelection();

        qInfo() << "INFO: [GitLogDialog] Loading more commits, current count:" << currentCount;
        m_dataManager->loadCommitHistory(currentBranch, currentCount, DEFAULT_COMMIT_LIMIT);
    }
}

void GitLogDialog::refreshAfterOperation()
{
    QTimer::singleShot(500, this, [this]() {
        onRefreshClicked();
    });
}

void GitLogDialog::selectFirstLocalCommit()
{
    if (!m_commitTree || m_commitTree->topLevelItemCount() == 0) {
        return;
    }

    const auto &commits = m_dataManager->getCommits();

    // === 修复：优先选中标记为isLocalHead的commit ===
    for (int i = 0; i < m_commitTree->topLevelItemCount() && i < commits.size(); ++i) {
        const auto &commit = commits[i];

        if (commit.isLocalHead) {
            // 找到本地HEAD commit，选中它
            QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
            m_commitTree->setCurrentItem(item);
            m_commitTree->scrollToItem(item);

            qInfo() << QString("INFO: [GitLogDialog] Auto-selected local HEAD commit at index %1: %2")
                               .arg(i)
                               .arg(commit.shortHash);
            return;
        }
    }

    // === 回退逻辑1：如果没有找到isLocalHead标记，寻找第一个本地commit ===
    for (int i = 0; i < m_commitTree->topLevelItemCount() && i < commits.size(); ++i) {
        const auto &commit = commits[i];

        if (commit.source == GitLogDataManager::CommitSource::Local || commit.source == GitLogDataManager::CommitSource::Both) {

            QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
            m_commitTree->setCurrentItem(item);
            m_commitTree->scrollToItem(item);

            qInfo() << QString("INFO: [GitLogDialog] Auto-selected first local commit at index %1: %2")
                               .arg(i)
                               .arg(commit.shortHash);
            return;
        }
    }

    // === 最终回退：选择第一个可用的commit ===
    QTreeWidgetItem *firstItem = m_commitTree->topLevelItem(0);
    m_commitTree->setCurrentItem(firstItem);
    m_commitTree->scrollToItem(firstItem);

    qInfo() << "INFO: [GitLogDialog] No local commits found, selected first available commit";
}

QIcon GitLogDialog::getFileStatusIcon(const QString &status) const
{
    if (status == "A") return QIcon::fromTheme("list-add");
    if (status == "M") return QIcon::fromTheme("document-edit");
    if (status == "D") return QIcon::fromTheme("list-remove");
    if (status == "R") return QIcon::fromTheme("edit-rename");
    if (status == "C") return QIcon::fromTheme("edit-copy");
    return QIcon::fromTheme("document-properties");
}

QIcon GitLogDialog::getRemoteStatusIcon(GitLogDataManager::RemoteStatus status) const
{
    switch (status) {
    case GitLogDataManager::RemoteStatus::Synchronized:
        return QIcon::fromTheme("emblem-default", QIcon(":/icons/status-synced.png"));
    case GitLogDataManager::RemoteStatus::Ahead:
        return QIcon::fromTheme("go-up", QIcon(":/icons/status-ahead.png"));
    case GitLogDataManager::RemoteStatus::Behind:
        return QIcon::fromTheme("go-down", QIcon(":/icons/status-behind.png"));
    case GitLogDataManager::RemoteStatus::Diverged:
        return QIcon::fromTheme("dialog-warning", QIcon(":/icons/status-diverged.png"));
    case GitLogDataManager::RemoteStatus::NotTracked:
        return QIcon::fromTheme("emblem-unreadable", QIcon(":/icons/status-untracked.png"));
    case GitLogDataManager::RemoteStatus::Unknown:
    default:
        return QIcon::fromTheme("help-about", QIcon(":/icons/status-unknown.png"));
    }
}

QColor GitLogDialog::getRemoteStatusColor(GitLogDataManager::RemoteStatus status) const
{
    switch (status) {
    case GitLogDataManager::RemoteStatus::Synchronized:
        return QColor(76, 175, 80);   // 绿色
    case GitLogDataManager::RemoteStatus::Ahead:
        return QColor(255, 193, 7);   // 黄色
    case GitLogDataManager::RemoteStatus::Behind:
        return QColor(244, 67, 54);   // 红色
    case GitLogDataManager::RemoteStatus::Diverged:
        return QColor(255, 152, 0);   // 橙色
    case GitLogDataManager::RemoteStatus::NotTracked:
        return QColor(158, 158, 158);   // 灰色
    case GitLogDataManager::RemoteStatus::Unknown:
    default:
        return QColor(189, 189, 189);   // 浅灰色
    }
}

QString GitLogDialog::getRemoteStatusTooltip(GitLogDataManager::RemoteStatus status, const QString &remoteRef) const
{
    QString baseText;
    switch (status) {
    case GitLogDataManager::RemoteStatus::Synchronized:
        baseText = tr("Synchronized with remote");
        break;
    case GitLogDataManager::RemoteStatus::Ahead:
        baseText = tr("Local commit ahead of remote");
        break;
    case GitLogDataManager::RemoteStatus::Behind:
        baseText = tr("Remote commit not in local branch");
        break;
    case GitLogDataManager::RemoteStatus::Diverged:
        baseText = tr("Branch has diverged from remote");
        break;
    case GitLogDataManager::RemoteStatus::NotTracked:
        baseText = tr("Branch is not tracking any remote");
        break;
    case GitLogDataManager::RemoteStatus::Unknown:
    default:
        baseText = tr("Remote status unknown");
        break;
    }

    if (!remoteRef.isEmpty()) {
        QString tooltip = QString("%1\nRemote: %2").arg(baseText, remoteRef);

        // 如果有多个远程分支信息，添加额外提示
        const auto &trackingInfo = m_dataManager->getBranchTrackingInfo();
        if (trackingInfo.allUpstreams.size() > 1) {
            tooltip += QString("\n\nMultiple upstreams available:");
            for (const QString &upstream : trackingInfo.allUpstreams) {
                if (upstream == remoteRef) {
                    tooltip += QString("\n• %1 (current)").arg(upstream);
                } else {
                    tooltip += QString("\n• %1").arg(upstream);
                }
            }
        }

        return tooltip;
    }
    return baseText;
}

QString GitLogDialog::getRemoteStatusText(GitLogDataManager::RemoteStatus status) const
{
    switch (status) {
    case GitLogDataManager::RemoteStatus::Synchronized:
        return "✓";
    case GitLogDataManager::RemoteStatus::Ahead:
        return "↑";
    case GitLogDataManager::RemoteStatus::Behind:
        return "↓";
    case GitLogDataManager::RemoteStatus::Diverged:
        return "⚠";
    case GitLogDataManager::RemoteStatus::NotTracked:
        return "○";
    case GitLogDataManager::RemoteStatus::Unknown:
    default:
        return "?";
    }
}

QString GitLogDialog::formatChangeStats(int additions, int deletions) const
{
    if (additions == 0 && deletions == 0) {
        return tr("No changes");
    }

    QString result;
    if (additions > 0) {
        result += QString("+%1").arg(additions);
    }
    if (deletions > 0) {
        if (!result.isEmpty()) {
            result += " ";
        }
        result += QString("-%1").arg(deletions);
    }

    return result;
}

void GitLogDialog::setChangeStatsColor(QTreeWidgetItem *item, int additions, int deletions) const
{
    if (!item) return;

    QColor textColor;
    if (additions > 0 && deletions == 0) {
        textColor = QColor(0, 128, 0);   // 绿色
    } else if (additions == 0 && deletions > 0) {
        textColor = QColor(128, 0, 0);   // 红色
    } else if (additions > 0 && deletions > 0) {
        textColor = QColor(255, 140, 0);   // 橙色
    } else {
        textColor = QColor(128, 128, 128);   // 灰色
    }

    item->setForeground(2, QBrush(textColor));

    // 设置工具提示
    QString tooltip = tr("Lines added: %1, Lines deleted: %2").arg(additions).arg(deletions);
    item->setToolTip(2, tooltip);
}

void GitLogDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space) {
        QString filePath = getCurrentSelectedFilePath();
        if (!filePath.isEmpty()) {
            if (m_currentPreviewDialog) {
                m_currentPreviewDialog->close();
                m_currentPreviewDialog = nullptr;
            } else {
                previewSelectedFile();
            }
        }
        event->accept();
        return;
    }

    QDialog::keyPressEvent(event);
}

bool GitLogDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_changedFilesTree && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Space) {
            QString filePath = getCurrentSelectedFilePath();
            if (!filePath.isEmpty()) {
                if (m_currentPreviewDialog) {
                    m_currentPreviewDialog->close();
                    m_currentPreviewDialog = nullptr;
                } else {
                    previewSelectedFile();
                }
                return true;
            }
        }
    }

    return QDialog::eventFilter(watched, event);
}

// === 语法高亮器实现 ===

GitDiffSyntaxHighlighter::GitDiffSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // 添加的行格式（绿色背景）
    m_addedLineFormat.setForeground(QColor(0, 128, 0));
    m_addedLineFormat.setBackground(QColor(230, 255, 230));

    // 删除的行格式（红色背景）
    m_removedLineFormat.setForeground(QColor(128, 0, 0));
    m_removedLineFormat.setBackground(QColor(255, 230, 230));

    // 行号信息格式（蓝色粗体）
    m_lineNumberFormat.setForeground(QColor(0, 0, 128));
    m_lineNumberFormat.setFontWeight(QFont::Bold);

    // 文件路径格式（紫色粗体）
    m_filePathFormat.setForeground(QColor(128, 0, 128));
    m_filePathFormat.setFontWeight(QFont::Bold);

    // 上下文行格式（深灰色）
    m_contextFormat.setForeground(QColor(64, 64, 64));
}

void GitDiffSyntaxHighlighter::highlightBlock(const QString &text)
{
    // 添加的行：以+开头，但不是+++
    if (text.startsWith('+') && !text.startsWith("+++")) {
        setFormat(0, text.length(), m_addedLineFormat);
    }
    // 删除的行：以-开头，但不是---
    else if (text.startsWith('-') && !text.startsWith("---")) {
        setFormat(0, text.length(), m_removedLineFormat);
    }
    // 行号信息：@@开头
    else if (text.startsWith("@@")) {
        setFormat(0, text.length(), m_lineNumberFormat);
    }
    // 文件路径：+++或---开头
    else if (text.startsWith("+++") || text.startsWith("---")) {
        setFormat(0, text.length(), m_filePathFormat);
    }
    // 上下文行：以空格开头
    else if (text.startsWith(" ")) {
        setFormat(0, text.length(), m_contextFormat);
    }
    // 其他行使用默认格式
}

QColor GitLogDialog::getCommitSourceColor(GitLogDataManager::CommitSource source) const
{
    switch (source) {
    case GitLogDataManager::CommitSource::Local:
        return QColor(0, 0, 0);   // 黑色（默认）
    case GitLogDataManager::CommitSource::Remote:
        return QColor(138, 43, 226);   // 紫色（类似VSCode）
    case GitLogDataManager::CommitSource::Both:
        return QColor(0, 100, 0);   // 深绿色
    default:
        return QColor(0, 0, 0);   // 默认黑色
    }
}

void GitLogDialog::loadCommitsForInitialBranch(const QString &branch)
{
    qInfo() << "INFO: [GitLogDialog] Loading commits for initial branch:" << branch;

    // === 关键修复：使用新的确保HEAD包含的加载方法 ===
    // 这个方法会确保本地HEAD commit包含在初始加载的commits中
    bool loadSuccess = m_dataManager->loadCommitHistoryEnsureHead(branch, 100);

    if (!loadSuccess) {
        qWarning() << "WARNING: [GitLogDialog] Failed to load commits ensuring HEAD, falling back to normal loading";

        // 回退到原来的逻辑
        m_dataManager->loadAllRemoteTrackingInfo(branch);

        if (m_dataManager->shouldLoadRemoteCommits(branch)) {
            qInfo() << "INFO: [GitLogDialog] Fallback: Loading with remote commits";
            m_dataManager->loadCommitHistoryWithRemote(branch);
        } else {
            qInfo() << "INFO: [GitLogDialog] Fallback: Loading regular commits";
            m_dataManager->loadCommitHistory(branch);
        }
    }

    // 延迟更新远程状态（让commits先加载）
    QTimer::singleShot(200, this, [this, branch]() {
        if (m_dataManager) {
            qInfo() << "INFO: [GitLogDialog] Updating remote status for initial branch:" << branch;
            m_dataManager->updateCommitRemoteStatus(branch);
        }
    });
}

// === 加载状态管理方法 ===

void GitLogDialog::showLoadingStatus(const QString &message)
{
    if (!m_loadingWidget || !m_loadingAnimation) {
        qWarning() << "WARNING: [GitLogDialog] Loading widgets not initialized";
        return;
    }

    qInfo() << "INFO: [GitLogDialog] Showing loading status:" << message;

    m_loadingAnimation->setBaseText(message);
    m_loadingAnimation->startAnimation();
    m_loadingWidget->setVisible(true);
}

void GitLogDialog::hideLoadingStatus()
{
    if (!m_loadingWidget || !m_loadingAnimation) {
        return;
    }

    qInfo() << "INFO: [GitLogDialog] Hiding loading status";

    m_loadingAnimation->stopAnimation();
    m_loadingWidget->setVisible(false);
}

// === 新增：浏览器打开commit功能实现 ===

void GitLogDialog::onOpenCommitInBrowserRequested(const QString &commitHash)
{
    if (commitHash.isEmpty()) {
        qWarning() << "WARNING: [GitLogDialog] Empty commit hash for browser open request";
        return;
    }

    qInfo() << "INFO: [GitLogDialog] Opening commit in browser:" << commitHash.left(8);
    openCommitInBrowser(commitHash);
}

QString GitLogDialog::getRepositoryName() const
{
    QDir repoDir(m_repositoryPath);
    QString repoName = repoDir.dirName();

    // 如果目录名为空或者是"."，尝试从路径中获取
    if (repoName.isEmpty() || repoName == ".") {
        QFileInfo repoInfo(m_repositoryPath);
        repoName = repoInfo.baseName();
    }

    // 如果仍然为空，使用默认名称
    if (repoName.isEmpty()) {
        repoName = tr("Repository");
    }

    return repoName;
}

QString GitLogDialog::getRemoteUrl(const QString &remoteName) const
{
    if (remoteName.isEmpty()) {
        return QString();
    }

    QStringList args = { "remote", "get-url", remoteName };
    QString output, error;

    if (!m_dataManager->executeGitCommand(args, output, error)) {
        qWarning() << "WARNING: [GitLogDialog::getRemoteUrl] Failed to get remote URL for" << remoteName << ":" << error;
        return QString();
    }

    QString url = output.trimmed();
    qInfo() << "INFO: [GitLogDialog::getRemoteUrl] Got remote URL for" << remoteName << ":" << url;
    return url;
}

QString GitLogDialog::buildCommitUrl(const QString &remoteUrl, const QString &commitHash) const
{
    if (remoteUrl.isEmpty() || commitHash.isEmpty()) {
        return QString();
    }

    QString url = remoteUrl;

    // 处理SSH URL格式 (git@github.com:user/repo.git)
    if (url.startsWith("git@")) {
        QRegularExpression sshRegex(R"(git@([^:]+):(.+)\.git)");
        QRegularExpressionMatch match = sshRegex.match(url);
        if (match.hasMatch()) {
            QString host = match.captured(1);
            QString path = match.captured(2);
            url = QString("https://%1/%2").arg(host, path);
        }
    }

    // 移除.git后缀
    if (url.endsWith(".git")) {
        url.chop(4);
    }

    // 根据不同的Git托管平台构建commit URL
    if (url.contains("github.com")) {
        return QString("%1/commit/%2").arg(url, commitHash);
    } else if (url.contains("gitlab.com") || url.contains("gitlab.")) {
        return QString("%1/-/commit/%2").arg(url, commitHash);
    } else if (url.contains("bitbucket.org")) {
        return QString("%1/commits/%2").arg(url, commitHash);
    } else if (url.contains("gitee.com")) {
        return QString("%1/commit/%2").arg(url, commitHash);
    } else if (url.contains("coding.net")) {
        return QString("%1/commit/%2").arg(url, commitHash);
    } else {
        // 对于其他Git服务，尝试通用格式
        return QString("%1/commit/%2").arg(url, commitHash);
    }
}

void GitLogDialog::openCommitInBrowser(const QString &commitHash)
{
    if (commitHash.isEmpty()) {
        return;
    }

    QString remoteUrl = getRemoteUrl("origin");

    if (remoteUrl.isEmpty()) {
        QMessageBox::warning(this, tr("No Remote URL"),
                             tr("Cannot open commit in browser: no remote URL found for 'origin'.\n\n"
                                "Please ensure this repository has a remote named 'origin'."));
        return;
    }

    QString commitUrl = buildCommitUrl(remoteUrl, commitHash);
    if (commitUrl.isEmpty()) {
        QMessageBox::warning(this, tr("Unsupported Remote"),
                             tr("Cannot open commit in browser: unsupported remote URL format.\n\n"
                                "Remote URL: %1")
                                     .arg(remoteUrl));
        return;
    }

    qInfo() << "INFO: [GitLogDialog::openCommitInBrowser] Opening commit URL:" << commitUrl;

    if (!QDesktopServices::openUrl(QUrl(commitUrl))) {
        QMessageBox::critical(this, tr("Failed to Open Browser"),
                              tr("Failed to open the commit URL in browser.\n\n"
                                 "URL: %1\n\n"
                                 "You can copy this URL manually.")
                                      .arg(commitUrl));
    }
}
