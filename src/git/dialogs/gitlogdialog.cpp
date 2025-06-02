#include "gitlogdialog.h"
#include "widgets/gitcommitdetailswidget.h"
#include "gitlogdatamanager.h"
#include "gitlogsearchmanager.h"
#include "gitlogcontextmenumanager.h"
#include "widgets/linenumbertextedit.h"
#include "widgets/searchablebranchselector.h"
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

    // 异步加载数据
    QTimer::singleShot(0, this, [this]() {
        m_dataManager->setFilePath(m_filePath);
        m_dataManager->loadBranches();
        m_dataManager->loadCommitHistory(m_initialBranch);
    });

    qInfo() << "INFO: [GitLogDialog] Refactored GitLogDialog initialized successfully";
}

void GitLogDialog::setupUI()
{
    setWindowTitle(m_filePath.isEmpty() ? tr("Git Log - Repository") : tr("Git Log - %1").arg(QFileInfo(m_filePath).fileName()));

    setModal(false);
    setMinimumSize(1200, 800);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(1600, 1000);
    setAttribute(Qt::WA_DeleteOnClose);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    setupToolbar();
    setupMainLayout();

    mainLayout->addLayout(m_toolbarLayout);
    mainLayout->addWidget(m_mainSplitter);

    setupInfiniteScroll();
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
                    m_contextMenuManager->showCommitContextMenu(
                            m_commitTree->mapToGlobal(pos), commitHash, commitMessage);
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

    // 右键菜单管理器信号
    connect(m_contextMenuManager, &GitLogContextMenuManager::gitOperationRequested,
            this, &GitLogDialog::onGitOperationRequested);
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
    m_dataManager->loadCommitHistory(m_branchSelector->getCurrentSelection());
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
                                    "• Improved maintainability and testability"));
    });

    settingsMenu.exec(m_settingsButton->mapToGlobal(QPoint(0, m_settingsButton->height())));
}

void GitLogDialog::onBranchSelectorChanged(const QString &branchName)
{
    qInfo() << "INFO: [GitLogDialog] Branch selector changed to:" << branchName;
    m_dataManager->clearCommitCache();
    m_dataManager->loadCommitHistory(branchName);
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

    // 如果是首次加载且有结果，选中第一项
    if (!append && m_commitTree->topLevelItemCount() > 0) {
        m_commitTree->setCurrentItem(m_commitTree->topLevelItem(0));
    }

    m_isLoadingMore = false;
}

void GitLogDialog::onBranchesLoaded(const GitLogDataManager::BranchInfo &branchInfo)
{
    m_branchSelector->setBranches(branchInfo.localBranches, branchInfo.remoteBranches,
                                  branchInfo.tags, branchInfo.currentBranch);

    // 设置初始分支
    if (!m_initialBranch.isEmpty()) {
        m_branchSelector->setCurrentSelection(m_initialBranch);
    } else if (!branchInfo.currentBranch.isEmpty()) {
        m_branchSelector->setCurrentSelection(branchInfo.currentBranch);
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

void GitLogDialog::onShowCommitDetailsRequested(const QString &commitHash)
{
    Q_UNUSED(commitHash)
    // 可以在这里实现专门的提交详情对话框
    QMessageBox::information(this, tr("Commit Details"),
                             tr("Detailed commit dialog will be implemented in future version.\n"
                                "Current commit: %1")
                                     .arg(commitHash));
}

void GitLogDialog::onShowFileDiffRequested(const QString &commitHash, const QString &filePath)
{
    // 创建专门的文件差异查看对话框
    QDialog *diffDialog = new QDialog(this);
    diffDialog->setWindowTitle(tr("File Diff - %1 at %2")
                                       .arg(QFileInfo(filePath).fileName(), commitHash.left(8)));
    diffDialog->resize(1000, 700);
    diffDialog->setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(diffDialog);

    // 添加文件信息标签
    auto *infoLabel = new QLabel(tr("File: %1\nCommit: %2").arg(filePath, commitHash));
    infoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 8px; border: 1px solid #ccc; }");
    layout->addWidget(infoLabel);

    // 创建差异显示区域
    auto *diffTextEdit = new LineNumberTextEdit;
    diffTextEdit->setReadOnly(true);
    diffTextEdit->setFont(QFont("Consolas", 9));
    diffTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(diffTextEdit);

    // 添加语法高亮
    auto *highlighter = new GitDiffSyntaxHighlighter(diffTextEdit->document());
    Q_UNUSED(highlighter)

    // 加载文件差异并显示
    QString diff = m_dataManager->getFileDiff(commitHash, filePath);
    if (diff.isEmpty()) {
        // 如果缓存中没有，异步加载
        connect(m_dataManager, &GitLogDataManager::fileDiffLoaded,
                this, [diffTextEdit](const QString &hash, const QString &path, const QString &diffContent) {
                    Q_UNUSED(hash)
                    Q_UNUSED(path)
                    diffTextEdit->setPlainText(diffContent);
                });
        m_dataManager->loadFileDiff(commitHash, filePath);
        diffTextEdit->setPlainText(tr("Loading file diff..."));
    } else {
        diffTextEdit->setPlainText(diff);
    }

    // 添加按钮
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    auto *closeButton = new QPushButton(tr("Close"));
    connect(closeButton, &QPushButton::clicked, diffDialog, &QDialog::accept);
    buttonLayout->addWidget(closeButton);

    layout->addLayout(buttonLayout);

    diffDialog->show();
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
    if (!append) {
        m_commitTree->clear();
    }

    for (const auto &commit : commits) {
        auto *item = new QTreeWidgetItem(m_commitTree);
        item->setText(0, commit.graphInfo);
        item->setText(1, commit.message);
        item->setText(2, commit.author);
        item->setText(3, commit.date);
        item->setText(4, commit.shortHash);
        item->setData(4, Qt::UserRole, commit.fullHash);

        // 设置工具提示
        item->setToolTip(0, commit.graphInfo == "●" ? "Commit" : commit.graphInfo);
        item->setToolTip(1, commit.message);
        item->setToolTip(4, commit.fullHash);
    }
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
        m_dataManager->loadCommitHistory(currentBranch, currentCount, 100);
    }
}

void GitLogDialog::refreshAfterOperation()
{
    QTimer::singleShot(500, this, [this]() {
        onRefreshClicked();
    });
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
