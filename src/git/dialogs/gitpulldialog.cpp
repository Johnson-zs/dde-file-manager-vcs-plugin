#include "gitpulldialog.h"
#include "gitoperationservice.h"
#include "gitcommandexecutor.h"
#include "widgets/characteranimationwidget.h"
#include "gitdialogs.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QListWidget>
#include <QProgressBar>
#include <QMessageBox>
#include <QTimer>
#include <QSplitter>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QMenu>
#include <QAction>
#include <QRegularExpression>
#include <QPlainTextEdit>
#include <QFont>
#include <QDebug>

GitPullDialog::GitPullDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_operationService(new GitOperationService(this)), m_hasLocalChanges(false), m_hasUncommittedChanges(false), m_isOperationInProgress(false), m_isDryRunInProgress(false), m_isDataLoaded(false), m_statusUpdateTimer(new QTimer(this)), m_updatesContextMenu(nullptr), m_openInBrowserAction(nullptr), m_copyHashAction(nullptr), m_copyMessageAction(nullptr), m_showDetailsAction(nullptr)
{
    setWindowTitle(tr("Git Pull"));
    setWindowIcon(QIcon(":/icons/vcs-pull"));
    setMinimumSize(800, 500);
    resize(900, 600);

    qInfo() << "INFO: [GitPullDialog] Initializing pull dialog for repository:" << repositoryPath;

    setupUI();
    setupConnections();

    // 延迟加载数据以提高启动速度
    QTimer::singleShot(0, this, &GitPullDialog::delayedDataLoad);

    // 设置定时器更新状态
    m_statusUpdateTimer->setSingleShot(false);
    m_statusUpdateTimer->setInterval(30000);   // 30秒更新一次
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &GitPullDialog::refreshRemoteUpdates);
    // 注意：延迟启动定时器，等数据加载完成后再启动
}

GitPullDialog::~GitPullDialog()
{
    qInfo() << "INFO: [GitPullDialog] Destroying pull dialog";
}

void GitPullDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 6);

    // 创建主分割器
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // 左侧面板
    auto *leftWidget = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setSpacing(8);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    setupStatusGroup();
    setupConfigGroup();

    leftLayout->addWidget(m_statusGroup);
    leftLayout->addWidget(m_configGroup);
    leftLayout->addStretch();

    // 右侧面板
    auto *rightWidget = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setSpacing(8);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    setupUpdatesGroup();
    rightLayout->addWidget(m_updatesGroup);

    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    mainLayout->addWidget(splitter);

    // 进度显示区域 - 重新设计布局以包含CharacterAnimationWidget
    auto *progressWidget = new QWidget;
    progressWidget->setFixedHeight(40);
    auto *progressLayout = new QVBoxLayout(progressWidget);
    progressLayout->setContentsMargins(16, 8, 16, 8);
    progressLayout->setSpacing(4);

    // 字符动画组件
    m_animationWidget = new CharacterAnimationWidget(progressWidget);
    m_animationWidget->setVisible(false);
    m_animationWidget->setTextStyleSheet("QLabel { color: #2196F3; font-weight: bold; font-size: 14px; }");
    progressLayout->addWidget(m_animationWidget);

    // 保留原有的进度条和标签（隐藏状态）
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    m_progressLabel = new QLabel;
    m_progressLabel->setVisible(false);

    mainLayout->addWidget(progressWidget);

    setupButtonGroup();

    // 优化按钮区域布局，保持按钮默认高度
    auto *buttonWidget = new QWidget;
    buttonWidget->setFixedHeight(50);   // 减少容器高度，让按钮有更多空间
    auto *buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setSpacing(6);
    buttonLayout->setContentsMargins(0, 8, 0, 8);   // 统一上下边距，让按钮居中

    // 远程管理按钮
    m_remoteManagerButton = new QPushButton(tr("Remote Manager"));
    m_remoteManagerButton->setIcon(QIcon(":/icons/vcs-branch"));
    m_remoteManagerButton->setToolTip(tr("Manage remote repositories"));

    // 获取更新按钮
    m_fetchButton = new QPushButton(tr("Fetch Updates"));
    m_fetchButton->setIcon(QIcon(":/icons/vcs-update-required"));
    m_fetchButton->setToolTip(tr("Fetch latest changes without merging"));

    // Stash & Pull按钮
    m_stashPullButton = new QPushButton(tr("Stash & Pull"));
    m_stashPullButton->setIcon(QIcon(":/icons/vcs-stash"));
    m_stashPullButton->setToolTip(tr("Stash local changes and pull"));

    // 试运行按钮
    m_dryRunButton = new QPushButton(tr("Dry Run"));
    m_dryRunButton->setIcon(QIcon(":/icons/vcs-status"));
    m_dryRunButton->setToolTip(tr("Test pull without actually pulling"));

    buttonLayout->addWidget(m_remoteManagerButton);
    buttonLayout->addWidget(m_fetchButton);
    buttonLayout->addWidget(m_stashPullButton);
    buttonLayout->addWidget(m_dryRunButton);
    buttonLayout->addStretch();

    // 主要操作按钮
    m_pullButton = new QPushButton(tr("Pull"));
    m_pullButton->setIcon(QIcon(":/icons/vcs-pull"));
    m_pullButton->setDefault(true);
    m_pullButton->setStyleSheet("QPushButton { font-weight: bold; }");   // 移除自定义padding

    m_cancelButton = new QPushButton(tr("Cancel"));
    m_cancelButton->setIcon(QIcon(":/icons/dialog-cancel"));

    buttonLayout->addWidget(m_pullButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addWidget(buttonWidget);

    // 设置右键菜单
    setupUpdatesContextMenu();
}

void GitPullDialog::setupStatusGroup()
{
    m_statusGroup = new QGroupBox(tr("Local Status"));
    auto *layout = new QGridLayout(m_statusGroup);
    layout->setSpacing(8);

    // 工作区状态
    layout->addWidget(new QLabel(tr("Working Tree:")), 0, 0);
    m_workingTreeLabel = new QLabel(tr("Checking..."));
    layout->addWidget(m_workingTreeLabel, 0, 1);

    // 暂存区状态
    layout->addWidget(new QLabel(tr("Staging Area:")), 1, 0);
    m_stagingAreaLabel = new QLabel(tr("Checking..."));
    layout->addWidget(m_stagingAreaLabel, 1, 1);

    // 本地更改
    layout->addWidget(new QLabel(tr("Local Changes:")), 2, 0);
    m_localChangesLabel = new QLabel(tr("Checking..."));
    layout->addWidget(m_localChangesLabel, 2, 1);

    // 当前分支
    layout->addWidget(new QLabel(tr("Current Branch:")), 3, 0);
    m_currentBranchLabel = new QLabel(tr("Loading..."));
    m_currentBranchLabel->setStyleSheet("font-weight: bold; color: #2196F3;");
    layout->addWidget(m_currentBranchLabel, 3, 1);

    layout->setColumnStretch(1, 1);
}

void GitPullDialog::setupConfigGroup()
{
    m_configGroup = new QGroupBox(tr("Pull Configuration"));
    auto *layout = new QGridLayout(m_configGroup);
    layout->setSpacing(8);

    // 远程仓库选择
    layout->addWidget(new QLabel(tr("Remote Repository:")), 0, 0);
    m_remoteCombo = new QComboBox;
    m_remoteCombo->setMinimumWidth(200);
    layout->addWidget(m_remoteCombo, 0, 1);

    // 远程分支
    layout->addWidget(new QLabel(tr("Remote Branch:")), 1, 0);
    m_remoteBranchCombo = new QComboBox;
    layout->addWidget(m_remoteBranchCombo, 1, 1);

    // 合并策略
    layout->addWidget(new QLabel(tr("Merge Strategy:")), 2, 0);
    m_strategyCombo = new QComboBox;
    m_strategyCombo->addItem(tr("Merge"), static_cast<int>(MergeStrategy::Merge));
    m_strategyCombo->addItem(tr("Rebase"), static_cast<int>(MergeStrategy::Rebase));
    m_strategyCombo->addItem(tr("Fast-forward only"), static_cast<int>(MergeStrategy::FastForwardOnly));
    layout->addWidget(m_strategyCombo, 2, 1);

    // 拉取选项
    m_ffOnlyCheckBox = new QCheckBox(tr("Fast-forward only (--ff-only)"));
    m_ffOnlyCheckBox->setToolTip(tr("Only allow fast-forward merges"));
    layout->addWidget(m_ffOnlyCheckBox, 3, 0, 1, 2);

    m_pruneCheckBox = new QCheckBox(tr("Prune remote branches (--prune)"));
    m_pruneCheckBox->setToolTip(tr("Remove remote-tracking branches that no longer exist"));
    layout->addWidget(m_pruneCheckBox, 4, 0, 1, 2);

    m_autoStashCheckBox = new QCheckBox(tr("Auto stash local changes"));
    m_autoStashCheckBox->setToolTip(tr("Automatically stash local changes before pull"));
    layout->addWidget(m_autoStashCheckBox, 5, 0, 1, 2);

    m_submodulesCheckBox = new QCheckBox(tr("Recurse submodules"));
    m_submodulesCheckBox->setToolTip(tr("Update submodules recursively"));
    layout->addWidget(m_submodulesCheckBox, 6, 0, 1, 2);

    layout->setColumnStretch(1, 1);
}

void GitPullDialog::setupUpdatesGroup()
{
    m_updatesGroup = new QGroupBox(tr("Remote Updates"));
    auto *layout = new QVBoxLayout(m_updatesGroup);
    layout->setSpacing(8);

    // 更新数量标签
    m_updatesCountLabel = new QLabel(tr("Loading updates..."));
    m_updatesCountLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_updatesCountLabel);

    // 下载统计
    m_downloadStatsLabel = new QLabel;
    layout->addWidget(m_downloadStatsLabel);

    // 更新列表
    m_updatesWidget = new QListWidget;
    m_updatesWidget->setAlternatingRowColors(true);
    m_updatesWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_updatesWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_updatesWidget);
}

void GitPullDialog::setupButtonGroup()
{
    // 按钮创建已移动到setupUI方法中，这里只保留空实现
}

void GitPullDialog::setupConnections()
{
    // 组合框信号
    connect(m_remoteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GitPullDialog::onRemoteChanged);
    connect(m_strategyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GitPullDialog::onStrategyChanged);

    // 复选框信号
    connect(m_autoStashCheckBox, &QCheckBox::toggled, this, &GitPullDialog::onAutoStashToggled);

    // 按钮信号
    connect(m_remoteManagerButton, &QPushButton::clicked, this, &GitPullDialog::showRemoteManager);
    connect(m_fetchButton, &QPushButton::clicked, this, &GitPullDialog::fetchUpdates);
    connect(m_stashPullButton, &QPushButton::clicked, this, &GitPullDialog::stashAndPull);
    connect(m_dryRunButton, &QPushButton::clicked, this, &GitPullDialog::executeDryRun);
    connect(m_pullButton, &QPushButton::clicked, this, &GitPullDialog::executePull);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    // 操作服务信号 - 修复参数匹配问题
    connect(m_operationService, &GitOperationService::operationCompleted,
            this, [this](const QString &operation, bool success, const QString &message) {
                Q_UNUSED(operation)
                onPullCompleted(success, message);
            });

    // 右键菜单信号
    connect(m_updatesWidget, &QListWidget::customContextMenuRequested,
            this, &GitPullDialog::showUpdatesContextMenu);

    // 双击列表项信号 - 打开浏览器
    connect(m_updatesWidget, &QListWidget::itemDoubleClicked,
            this, &GitPullDialog::onItemDoubleClicked);
}

void GitPullDialog::loadRepositoryInfo()
{
    qInfo() << "INFO: [GitPullDialog::loadRepositoryInfo] Loading repository information";

    loadRemotes();
    loadBranches();
    checkLocalChanges();
    loadRemoteUpdates();
    updateLocalStatus();
}

void GitPullDialog::loadRemotes()
{
    GitCommandExecutor executor;
    QString output, error;

    GitCommandExecutor::GitCommand cmd;
    cmd.command = "remote";
    cmd.arguments = QStringList() << "remote";
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);

    m_remotes.clear();
    m_remoteCombo->clear();

    if (result == GitCommandExecutor::Result::Success) {
        m_remotes = output.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                 Qt::SkipEmptyParts
#else
                                 QString::SkipEmptyParts
#endif
        );
        m_remoteCombo->addItems(m_remotes);

        // 默认选择origin
        int originIndex = m_remotes.indexOf("origin");
        if (originIndex >= 0) {
            m_remoteCombo->setCurrentIndex(originIndex);
        }

        qInfo() << "INFO: [GitPullDialog::loadRemotes] Loaded" << m_remotes.size() << "remotes";
    } else {
        qWarning() << "WARNING: [GitPullDialog::loadRemotes] Failed to load remotes:" << error;
        m_remoteCombo->addItem(tr("No remotes found"));
        m_remoteCombo->setEnabled(false);
    }
}

void GitPullDialog::loadBranches()
{
    GitCommandExecutor executor;
    QString output, error;

    // 获取当前分支
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "branch";
    cmd.arguments = QStringList { "symbolic-ref", "--short", "HEAD" };
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);

    if (result == GitCommandExecutor::Result::Success) {
        m_currentBranch = output.trimmed();
        m_currentBranchLabel->setText(m_currentBranch);

        qInfo() << "INFO: [GitPullDialog::loadBranches] Current branch:" << m_currentBranch;
    } else {
        qWarning() << "WARNING: [GitPullDialog::loadBranches] Failed to get current branch:" << error;
        m_currentBranch = "unknown";
        m_currentBranchLabel->setText(tr("Unknown"));
    }

    // 加载远程分支
    if (!m_remoteCombo->currentText().isEmpty()) {
        loadRemoteBranches();
    }
}

void GitPullDialog::loadRemoteBranches()
{
    if (m_remoteCombo->currentText().isEmpty()) {
        return;
    }

    GitCommandExecutor executor;
    QString output, error;

    GitCommandExecutor::GitCommand cmd;
    cmd.command = "branch";
    cmd.arguments = QStringList() << "branch"
                                  << "-r";
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);

    m_remoteBranches.clear();
    m_remoteBranchCombo->clear();

    if (result == GitCommandExecutor::Result::Success) {
        QStringList lines = output.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                         Qt::SkipEmptyParts
#else
                                         QString::SkipEmptyParts
#endif
        );
        QString currentRemote = m_remoteCombo->currentText();

        for (const QString &line : lines) {
            QString branchName = line.trimmed();
            if (branchName.startsWith(currentRemote + "/")) {
                QString remoteBranch = branchName.mid(currentRemote.length() + 1);
                if (remoteBranch != "HEAD") {
                    m_remoteBranches.append(remoteBranch);
                }
            }
        }

        m_remoteBranchCombo->addItems(m_remoteBranches);

        // 默认选择与当前本地分支同名的远程分支
        int matchIndex = m_remoteBranches.indexOf(m_currentBranch);
        if (matchIndex >= 0) {
            m_remoteBranchCombo->setCurrentIndex(matchIndex);
        }

        qInfo() << "INFO: [GitPullDialog::loadRemoteBranches] Loaded" << m_remoteBranches.size() << "remote branches";
    }
}

void GitPullDialog::checkLocalChanges()
{
    GitCommandExecutor executor;
    QString output, error;

    // 检查工作区状态
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "status";
    cmd.arguments = QStringList() << "status"
                                  << "--porcelain";
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);

    if (result == GitCommandExecutor::Result::Success) {
        QStringList lines = output.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                         Qt::SkipEmptyParts
#else
                                         QString::SkipEmptyParts
#endif
        );

        m_hasLocalChanges = !lines.isEmpty();
        m_hasUncommittedChanges = false;

        int modifiedFiles = 0;
        int stagedFiles = 0;

        for (const QString &line : lines) {
            if (line.length() >= 2) {
                QChar staged = line[0];
                QChar worktree = line[1];

                if (staged != ' ' && staged != '?') {
                    stagedFiles++;
                }
                if (worktree != ' ' && worktree != '?') {
                    modifiedFiles++;
                    m_hasUncommittedChanges = true;
                }
            }
        }

        // 更新状态标签
        if (modifiedFiles > 0) {
            m_workingTreeLabel->setText(tr("%1 modified files").arg(modifiedFiles));
            m_workingTreeLabel->setStyleSheet("color: #FF9800;");
        } else {
            m_workingTreeLabel->setText(tr("Clean"));
            m_workingTreeLabel->setStyleSheet("color: #4CAF50;");
        }

        if (stagedFiles > 0) {
            m_stagingAreaLabel->setText(tr("%1 staged files").arg(stagedFiles));
            m_stagingAreaLabel->setStyleSheet("color: #2196F3;");
        } else {
            m_stagingAreaLabel->setText(tr("Empty"));
            m_stagingAreaLabel->setStyleSheet("color: #4CAF50;");
        }

        if (m_hasLocalChanges) {
            m_localChangesLabel->setText(tr("Has changes"));
            m_localChangesLabel->setStyleSheet("color: #FF9800;");
        } else {
            m_localChangesLabel->setText(tr("No changes"));
            m_localChangesLabel->setStyleSheet("color: #4CAF50;");
        }

        qInfo() << "INFO: [GitPullDialog::checkLocalChanges] Local changes:" << m_hasLocalChanges
                << "Uncommitted:" << m_hasUncommittedChanges;
    } else {
        qWarning() << "WARNING: [GitPullDialog::checkLocalChanges] Failed to check status:" << error;
    }
}

void GitPullDialog::loadRemoteUpdates()
{
    // 将简化实现替换为真实实现
    loadActualRemoteUpdates();
}

// 槽函数实现
void GitPullDialog::onRemoteChanged()
{
    qInfo() << "INFO: [GitPullDialog::onRemoteChanged] Remote changed to:" << m_remoteCombo->currentText();
    loadRemoteBranches();
    loadRemoteUpdates();
}

void GitPullDialog::onStrategyChanged()
{
    auto strategy = static_cast<MergeStrategy>(m_strategyCombo->currentData().toInt());

    // 根据策略调整选项
    if (strategy == MergeStrategy::FastForwardOnly) {
        m_ffOnlyCheckBox->setChecked(true);
        m_ffOnlyCheckBox->setEnabled(false);
    } else {
        m_ffOnlyCheckBox->setEnabled(true);
    }
}

void GitPullDialog::onAutoStashToggled(bool enabled)
{
    if (enabled && m_hasUncommittedChanges) {
        QMessageBox::information(this, tr("Auto Stash"),
                                 tr("Local changes will be automatically stashed before pull and restored after."));
    }
}

void GitPullDialog::showRemoteManager()
{
    qInfo() << "INFO: [GitPullDialog::showRemoteManager] Opening remote manager";

    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitPullDialog::showRemoteManager] No QApplication instance found";
        return;
    }

    // 使用GitDialogManager显示远程管理对话框
    GitDialogManager::instance()->showRemoteManager(m_repositoryPath, this);
}

void GitPullDialog::fetchUpdates()
{
    qInfo() << "INFO: [GitPullDialog::fetchUpdates] Starting fetch operation";

    if (m_isOperationInProgress) {
        QMessageBox::information(this, tr("Operation in Progress"),
                                 tr("Another operation is currently in progress. Please wait."));
        return;
    }

    // 显示字符动画进度
    showProgress(tr("Fetching remote updates..."));

    // 创建异步执行器
    auto *executor = new GitCommandExecutor(this);

    // 连接异步完成信号
    connect(executor, &GitCommandExecutor::commandFinished,
            this, &GitPullDialog::onFetchCommandFinished);

    // 准备fetch命令
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "fetch";
    cmd.arguments = QStringList() << "fetch"
                                  << "--all";

    if (m_pruneCheckBox->isChecked()) {
        cmd.arguments << "--prune";
    }

    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 30000;   // 30秒超时

    // 异步执行
    executor->executeCommandAsync(cmd);
}

void GitPullDialog::onFetchCommandFinished(const QString &command, GitCommandExecutor::Result result, const QString &output, const QString &error)
{
    Q_UNUSED(command)

    // 删除临时执行器
    sender()->deleteLater();

    // 隐藏进度
    hideProgress();

    if (result == GitCommandExecutor::Result::Success) {
        qInfo() << "INFO: [GitPullDialog::onFetchCommandFinished] Fetch completed successfully";

        // 显示成功状态
        m_animationWidget->setVisible(true);
        m_animationWidget->getLabel()->setText(tr("✓ Fetch completed successfully!"));
        m_animationWidget->getLabel()->setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; font-size: 14px; }");

        // 刷新远程分支和更新列表
        loadRemoteBranches();
        loadActualRemoteUpdates();

        // 1.5秒后隐藏成功消息
        QTimer::singleShot(1500, this, [this]() {
            m_animationWidget->setVisible(false);
        });

    } else {
        qWarning() << "WARNING: [GitPullDialog::onFetchCommandFinished] Fetch failed:" << error;

        // 显示错误状态
        m_animationWidget->setVisible(true);
        m_animationWidget->getLabel()->setText(tr("✗ Fetch failed!"));
        m_animationWidget->getLabel()->setStyleSheet("QLabel { color: #F44336; font-weight: bold; font-size: 14px; }");

        QMessageBox::critical(this, tr("Fetch Failed"),
                              tr("Failed to fetch remote updates.\n\nError: %1").arg(error));

        // 3秒后隐藏错误消息
        QTimer::singleShot(3000, this, [this]() {
            m_animationWidget->setVisible(false);
        });
    }
}

void GitPullDialog::stashAndPull()
{
    qInfo() << "INFO: [GitPullDialog::stashAndPull] Starting stash and pull operation";

    if (!m_hasLocalChanges) {
        QMessageBox::information(this, tr("No Changes"),
                                 tr("No local changes to stash. Proceeding with normal pull."));
        executePull();
        return;
    }

    if (m_isOperationInProgress) {
        QMessageBox::information(this, tr("Operation in Progress"),
                                 tr("Another operation is currently in progress. Please wait."));
        return;
    }

    // 确认操作
    int ret = QMessageBox::question(this, tr("Stash and Pull"),
                                    tr("This will:\n"
                                       "1. Stash your local changes\n"
                                       "2. Pull from remote repository\n"
                                       "3. Restore your stashed changes\n\n"
                                       "Do you want to continue?"),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }

    showProgress(tr("Starting stash and pull operation..."));

    GitCommandExecutor executor;
    QString output, error;
    QString stashMessage = tr("Auto-stash before pull at %1").arg(QDateTime::currentDateTime().toString());

    // 步骤1: Stash本地更改
    m_animationWidget->setBaseText(tr("Step 1/3: Stashing local changes..."));

    GitCommandExecutor::GitCommand stashCmd;
    stashCmd.command = "stash";
    stashCmd.arguments = QStringList() << "stash"
                                       << "push"
                                       << "-m" << stashMessage;
    stashCmd.workingDirectory = m_repositoryPath;
    stashCmd.timeout = 10000;

    auto stashResult = executor.executeCommand(stashCmd, output, error);

    if (stashResult != GitCommandExecutor::Result::Success) {
        hideProgress();

        QMessageBox::critical(this, tr("Stash Failed"),
                              tr("Failed to stash local changes.\n\nError: %1").arg(error));
        return;
    }

    // 步骤2: 执行Pull
    m_animationWidget->setBaseText(tr("Step 2/3: Pulling from remote..."));

    PullOptions options;
    options.remoteName = m_remoteCombo->currentText();
    options.remoteBranch = m_remoteBranchCombo->currentText();
    options.strategy = static_cast<MergeStrategy>(m_strategyCombo->currentData().toInt());
    options.fastForwardOnly = m_ffOnlyCheckBox->isChecked();
    options.prune = m_pruneCheckBox->isChecked();
    options.autoStash = false;   // 我们手动处理stash
    options.recurseSubmodules = m_submodulesCheckBox->isChecked();
    options.dryRun = false;

    // 构建pull命令
    QStringList pullArgs = { "pull" };

    if (options.prune) {
        pullArgs << "--prune";
    }

    QString strategyStr;
    switch (options.strategy) {
    case MergeStrategy::Merge:
        strategyStr = "merge";
        break;
    case MergeStrategy::Rebase:
        pullArgs << "--rebase";
        break;
    case MergeStrategy::FastForwardOnly:
        pullArgs << "--ff-only";
        break;
    }

    if (options.recurseSubmodules) {
        pullArgs << "--recurse-submodules";
    }

    pullArgs << options.remoteName;
    if (!options.remoteBranch.isEmpty()) {
        pullArgs << options.remoteBranch;
    }

    GitCommandExecutor::GitCommand pullCmd;
    pullCmd.command = "pull";
    pullCmd.arguments = pullArgs;
    pullCmd.workingDirectory = m_repositoryPath;
    pullCmd.timeout = 60000;   // 60秒超时

    auto pullResult = executor.executeCommand(pullCmd, output, error);

    // 步骤3: 恢复Stash (不管pull是否成功都要尝试)
    m_animationWidget->setBaseText(tr("Step 3/3: Restoring stashed changes..."));

    GitCommandExecutor::GitCommand popCmd;
    popCmd.command = "stash";
    popCmd.arguments = QStringList() << "stash"
                                     << "pop";
    popCmd.workingDirectory = m_repositoryPath;
    popCmd.timeout = 10000;

    auto popResult = executor.executeCommand(popCmd, output, error);

    // 完成操作
    hideProgress();

    // 检查结果并给出适当反馈
    if (pullResult == GitCommandExecutor::Result::Success) {
        if (popResult == GitCommandExecutor::Result::Success) {
            qInfo() << "INFO: [GitPullDialog::stashAndPull] Stash and pull completed successfully";

            // 显示成功状态
            m_animationWidget->setVisible(true);
            m_animationWidget->getLabel()->setText(tr("✓ Stash and pull completed successfully!"));
            m_animationWidget->getLabel()->setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; font-size: 14px; }");

            // 刷新状态
            checkLocalChanges();
            updateLocalStatus();

            // 延迟1.5秒后关闭对话框
            QTimer::singleShot(1500, this, [this]() {
                accept();
            });
        } else {
            // Pull成功但stash pop失败 - 可能有冲突
            QMessageBox::warning(this, tr("Stash Restore Failed"),
                                 tr("Pull completed successfully, but failed to restore stashed changes.\n"
                                    "Your changes are safely stored in the stash.\n\n"
                                    "You can manually restore them using:\n"
                                    "git stash pop\n\n"
                                    "Error: %1")
                                         .arg(error));
            handleConflicts();
        }
    } else {
        // Pull失败 - 尝试恢复原状态
        QMessageBox::critical(this, tr("Pull Failed"),
                              tr("Pull operation failed, but your changes have been preserved in the stash.\n"
                                 "You can restore them using: git stash pop\n\n"
                                 "Pull error: %1")
                                      .arg(error));
    }
}

void GitPullDialog::executeDryRun()
{
    qInfo() << "INFO: [GitPullDialog::executeDryRun] Starting dry run";

    if (m_hasUncommittedChanges && !m_autoStashCheckBox->isChecked()) {
        int ret = QMessageBox::question(this, tr("Uncommitted Changes"),
                                        tr("You have uncommitted changes that may be overwritten.\n"
                                           "Do you want to continue with dry run?"),
                                        QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) {
            return;
        }
    }

    PullOptions options;
    options.remoteName = m_remoteCombo->currentText();
    options.remoteBranch = m_remoteBranchCombo->currentText();
    options.strategy = static_cast<MergeStrategy>(m_strategyCombo->currentData().toInt());
    options.fastForwardOnly = m_ffOnlyCheckBox->isChecked();
    options.prune = m_pruneCheckBox->isChecked();
    options.autoStash = m_autoStashCheckBox->isChecked();
    options.recurseSubmodules = m_submodulesCheckBox->isChecked();
    options.dryRun = true;

    executePullWithOptions(options);
}

void GitPullDialog::executePull()
{
    qInfo() << "INFO: [GitPullDialog::executePull] Starting pull operation";

    if (m_hasUncommittedChanges && !m_autoStashCheckBox->isChecked()) {
        int ret = QMessageBox::question(this, tr("Uncommitted Changes"),
                                        tr("You have uncommitted changes that may be overwritten.\n"
                                           "Do you want to continue?"),
                                        QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) {
            return;
        }
    }

    PullOptions options;
    options.remoteName = m_remoteCombo->currentText();
    options.remoteBranch = m_remoteBranchCombo->currentText();
    options.strategy = static_cast<MergeStrategy>(m_strategyCombo->currentData().toInt());
    options.fastForwardOnly = m_ffOnlyCheckBox->isChecked();
    options.prune = m_pruneCheckBox->isChecked();
    options.autoStash = m_autoStashCheckBox->isChecked();
    options.recurseSubmodules = m_submodulesCheckBox->isChecked();
    options.dryRun = false;

    executePullWithOptions(options);
}

void GitPullDialog::executePullWithOptions(const PullOptions &options)
{
    m_isDryRunInProgress = options.dryRun;
    showProgress(options.dryRun ? tr("Running pull dry run...") : tr("Pulling from remote repository..."));

    qInfo() << "INFO: [GitPullDialog::executePullWithOptions] Executing pull with options:"
            << "remote:" << options.remoteName << "branch:" << options.remoteBranch
            << "strategy:" << static_cast<int>(options.strategy) << "ff-only:" << options.fastForwardOnly
            << "prune:" << options.prune << "auto-stash:" << options.autoStash
            << "submodules:" << options.recurseSubmodules << "dry-run:" << options.dryRun;

    // 转换合并策略为字符串
    QString strategyStr;
    switch (options.strategy) {
    case MergeStrategy::Merge:
        strategyStr = "merge";
        break;
    case MergeStrategy::Rebase:
        strategyStr = "rebase";
        break;
    case MergeStrategy::FastForwardOnly:
        strategyStr = "ff-only";
        break;
    }

    // 使用GitOperationService执行拉取
    m_operationService->pullWithOptions(m_repositoryPath, options.remoteName,
                                        options.remoteBranch, strategyStr,
                                        options.prune, options.autoStash, options.dryRun);
}

void GitPullDialog::onPullCompleted(bool success, const QString &message)
{
    bool isDryRun = m_isDryRunInProgress;

    m_isDryRunInProgress = false;
    hideProgress();

    if (success) {
        qInfo() << "INFO: [GitPullDialog::onPullCompleted] Pull completed successfully";

        if (isDryRun) {
            // Dry run成功时显示结果信息
            QMessageBox::information(this, tr("Dry Run Successful"),
                                     tr("Dry run completed successfully. No changes were made.\n\n%1").arg(message));
        } else {
            // 实际pull成功时直接关闭对话框，不显示弹窗
            qInfo() << "INFO: [GitPullDialog::onPullCompleted] Pull operation completed successfully, closing dialog";

            // 显示成功状态
            m_animationWidget->setVisible(true);
            m_animationWidget->getLabel()->setText(tr("✓ Pull completed successfully!"));
            m_animationWidget->getLabel()->setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; font-size: 14px; }");

            // 刷新状态
            checkLocalChanges();
            updateLocalStatus();

            // 延迟1.5秒后关闭对话框，让用户看到成功提示
            QTimer::singleShot(1500, this, [this]() {
                accept();
            });
        }
    } else {
        qWarning() << "WARNING: [GitPullDialog::onPullCompleted] Pull failed:" << message;

        if (isDryRun) {
            QMessageBox::critical(this, tr("Dry Run Failed"),
                                  tr("Dry run failed.\n\n%1").arg(message));
        } else {
            QMessageBox::critical(this, tr("Pull Failed"),
                                  tr("Pull operation failed.\n\n%1").arg(message));
        }
    }
}

void GitPullDialog::handleConflicts()
{
    qInfo() << "INFO: [GitPullDialog::handleConflicts] Checking for conflicts";

    // 检查是否有合并冲突
    GitCommandExecutor executor;
    QString output, error;

    GitCommandExecutor::GitCommand cmd;
    cmd.command = "status";
    cmd.arguments = QStringList() << "status"
                                  << "--porcelain";
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);

    if (result != GitCommandExecutor::Result::Success) {
        QMessageBox::critical(this, tr("Status Check Failed"),
                              tr("Failed to check repository status.\n\nError: %1").arg(error));
        return;
    }

    QStringList conflictFiles;
    QStringList statusLines = output.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                           Qt::SkipEmptyParts
#else
                                           QString::SkipEmptyParts
#endif
    );

    for (const QString &line : statusLines) {
        if (line.length() >= 2) {
            // 检查冲突标记 (UU, AA, DD, AU, UA, DU, UD)
            QString status = line.left(2);
            if (status == "UU" || status == "AA" || status == "DD" || status == "AU" || status == "UA" || status == "DU" || status == "UD") {
                QString filePath = line.mid(3);
                conflictFiles.append(filePath);
            }
        }
    }

    if (conflictFiles.isEmpty()) {
        // 没有冲突，可能是其他问题
        QMessageBox::information(this, tr("No Conflicts Detected"),
                                 tr("No merge conflicts were detected.\n"
                                    "The issue might be resolved automatically or require manual intervention.\n\n"
                                    "You can check the repository status using the Status dialog."));
        return;
    }

    // 有冲突，显示冲突处理指导
    QString conflictList = conflictFiles.join("\n• ");
    QMessageBox::StandardButton choice = QMessageBox::question(this, tr("Merge Conflicts Detected"),
                                                               tr("The following files have merge conflicts:\n\n• %1\n\n"
                                                                  "You need to resolve these conflicts manually.\n\n"
                                                                  "Would you like to:\n"
                                                                  "• Yes: Open Status dialog to resolve conflicts\n"
                                                                  "• No: Abort the merge and return to previous state\n"
                                                                  "• Cancel: Handle conflicts manually later")
                                                                       .arg(conflictList),
                                                               QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    switch (choice) {
    case QMessageBox::Yes:
        // 打开状态对话框帮助用户解决冲突
        qInfo() << "INFO: [GitPullDialog::handleConflicts] Opening status dialog for conflict resolution";
        if (QApplication::instance()) {
            QWidget *parentWidget = QApplication::activeWindow();
            GitDialogManager::instance()->showStatusDialog(m_repositoryPath, parentWidget);
        }
        break;

    case QMessageBox::No:
        // 中止合并
        abortMerge();
        break;

    default:
        // 用户选择手动处理，给出指导信息
        QMessageBox::information(this, tr("Manual Conflict Resolution"),
                                 tr("To resolve conflicts manually:\n\n"
                                    "1. Edit the conflicted files to resolve conflicts\n"
                                    "2. Remove conflict markers (<<<<<<< ======= >>>>>>>)\n"
                                    "3. Add resolved files: git add <file>\n"
                                    "4. Complete the merge: git commit\n\n"
                                    "Or abort the merge: git merge --abort"));
        break;
    }
}

void GitPullDialog::updateLocalStatus()
{
    validatePullOptions();
}

void GitPullDialog::refreshRemoteUpdates()
{
    if (!m_isOperationInProgress && m_isDataLoaded) {
        qInfo() << "INFO: [GitPullDialog::refreshRemoteUpdates] Refreshing remote updates";
        loadRemoteUpdates();
    }
}

void GitPullDialog::validatePullOptions()
{
    bool canPull = !m_remoteCombo->currentText().isEmpty() && !m_remoteBranchCombo->currentText().isEmpty() && !m_isOperationInProgress;

    m_pullButton->setEnabled(canPull);
    m_dryRunButton->setEnabled(canPull);
    m_fetchButton->setEnabled(canPull);
    m_stashPullButton->setEnabled(canPull && m_hasLocalChanges);
}

void GitPullDialog::enableControls(bool enabled)
{
    m_remoteCombo->setEnabled(enabled);
    m_remoteBranchCombo->setEnabled(enabled);
    m_strategyCombo->setEnabled(enabled);
    m_ffOnlyCheckBox->setEnabled(enabled);
    m_pruneCheckBox->setEnabled(enabled);
    m_autoStashCheckBox->setEnabled(enabled);
    m_submodulesCheckBox->setEnabled(enabled);
    m_remoteManagerButton->setEnabled(enabled);
    m_fetchButton->setEnabled(enabled);
    m_stashPullButton->setEnabled(enabled);
    m_dryRunButton->setEnabled(enabled);
    m_pullButton->setEnabled(enabled);

    // 在启用控件后，重新验证按钮状态
    if (enabled) {
        validatePullOptions();
    }
}

QString GitPullDialog::formatUpdateInfo(const RemoteUpdateInfo &update) const
{
    return QString("↓ %1 %2").arg(update.shortHash, update.message);
}

QString GitPullDialog::getLocalStatusDescription() const
{
    if (!m_hasLocalChanges) {
        return tr("Working tree is clean");
    } else if (m_hasUncommittedChanges) {
        return tr("Has uncommitted changes");
    } else {
        return tr("Has staged changes");
    }
}

QString GitPullDialog::getMergeStrategyDescription(MergeStrategy strategy) const
{
    switch (strategy) {
    case MergeStrategy::Merge:
        return tr("Create a merge commit");
    case MergeStrategy::Rebase:
        return tr("Rebase local commits");
    case MergeStrategy::FastForwardOnly:
        return tr("Only fast-forward merges");
    default:
        return tr("Unknown strategy");
    }
}

void GitPullDialog::loadActualRemoteUpdates()
{
    if (m_remoteCombo->currentText().isEmpty() || m_remoteBranchCombo->currentText().isEmpty()) {
        return;
    }

    GitCommandExecutor executor;
    QString output, error;

    QString remoteBranch = m_remoteCombo->currentText() + "/" + m_remoteBranchCombo->currentText();
    QString localBranch = m_currentBranch;

    // 获取远程更新 (远程有但本地没有的提交)
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "log";
    cmd.arguments = QStringList() << "log"
                                  << "--oneline"
                                  << "--no-merges"
                                  << QString("%1..%2").arg(localBranch, remoteBranch);
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 10000;

    auto result = executor.executeCommand(cmd, output, error);

    m_remoteUpdates.clear();
    m_updatesWidget->clear();

    if (result == GitCommandExecutor::Result::Success) {
        QStringList lines = output.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                         Qt::SkipEmptyParts
#else
                                         QString::SkipEmptyParts
#endif
        );

        if (lines.isEmpty()) {
            m_updatesCountLabel->setText(tr("No remote updates available"));
            m_downloadStatsLabel->setText(tr("Your branch is up to date"));

            auto *item = new QListWidgetItem(tr("No new commits on remote branch"));
            item->setIcon(QIcon(":/icons/vcs-normal"));
            m_updatesWidget->addItem(item);
        } else {
            m_updatesCountLabel->setText(tr("%1 remote updates available").arg(lines.size()));
            m_downloadStatsLabel->setText(tr("📊 Ready to download: %1 commits").arg(lines.size()));

            for (const QString &line : lines) {
                if (line.trimmed().isEmpty()) continue;

                QStringList parts = line.split(' ',
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                               Qt::SkipEmptyParts
#else
                                               QString::SkipEmptyParts
#endif
                );
                if (parts.size() >= 2) {
                    QString hash = parts[0];
                    QString message = parts.mid(1).join(' ');

                    RemoteUpdateInfo updateInfo;
                    updateInfo.hash = hash;
                    updateInfo.shortHash = hash.left(7);
                    updateInfo.message = message;
                    updateInfo.remoteBranch = remoteBranch;
                    m_remoteUpdates.append(updateInfo);

                    auto *item = new QListWidgetItem(formatUpdateInfo(updateInfo));
                    item->setIcon(QIcon(":/icons/vcs-update-required"));
                    item->setToolTip(tr("Hash: %1\nBranch: %2").arg(hash, remoteBranch));
                    m_updatesWidget->addItem(item);
                }
            }
        }

        qInfo() << "INFO: [GitPullDialog::loadActualRemoteUpdates] Loaded" << lines.size() << "remote updates";
    } else {
        qWarning() << "WARNING: [GitPullDialog::loadActualRemoteUpdates] Failed to load remote updates:" << error;

        m_updatesCountLabel->setText(tr("Failed to check remote updates"));
        m_downloadStatsLabel->setText(tr("Error: %1").arg(error));

        auto *item = new QListWidgetItem(tr("Failed to fetch remote updates"));
        item->setIcon(QIcon(":/icons/vcs-conflicted"));
        m_updatesWidget->addItem(item);
    }
}

void GitPullDialog::abortMerge()
{
    qInfo() << "INFO: [GitPullDialog::abortMerge] Aborting merge operation";

    GitCommandExecutor executor;
    QString output, error;

    GitCommandExecutor::GitCommand cmd;
    cmd.command = "merge";
    cmd.arguments = QStringList() << "merge"
                                  << "--abort";
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 10000;

    auto result = executor.executeCommand(cmd, output, error);

    if (result == GitCommandExecutor::Result::Success) {
        QMessageBox::information(this, tr("Merge Aborted"),
                                 tr("The merge has been aborted successfully.\n"
                                    "Your repository has been restored to the previous state."));

        // 刷新状态
        checkLocalChanges();
        updateLocalStatus();

        qInfo() << "INFO: [GitPullDialog::abortMerge] Merge aborted successfully";
    } else {
        QMessageBox::critical(this, tr("Abort Failed"),
                              tr("Failed to abort the merge.\n\nError: %1\n\n"
                                 "You may need to resolve this manually.")
                                      .arg(error));

        qWarning() << "WARNING: [GitPullDialog::abortMerge] Failed to abort merge:" << error;
    }
}

void GitPullDialog::delayedDataLoad()
{
    qInfo() << "INFO: [GitPullDialog::delayedDataLoad] Starting delayed data loading";

    // 显示加载状态
    showProgress(tr("Loading repository information..."));

    // 增加延迟时间，确保动画开始运行
    QTimer::singleShot(200, this, &GitPullDialog::loadRepositoryInfoAsync);
}

void GitPullDialog::loadRepositoryInfoAsync()
{
    // 在单独的方法中进行异步数据加载
    // 这样可以避免阻塞UI线程
    QTimer::singleShot(100, this, [this]() {
        loadRepositoryInfo();

        // 加载完成后隐藏进度动画
        hideProgress();
        m_isDataLoaded = true;

        // 现在启动定时器
        m_statusUpdateTimer->start();

        qInfo() << "INFO: [GitPullDialog::loadRepositoryInfoAsync] Repository data loaded successfully";
    });
}

void GitPullDialog::showProgress(const QString &message)
{
    m_isOperationInProgress = true;
    enableControls(false);

    // 使用字符动画替代进度条
    m_progressBar->setVisible(false);
    m_progressLabel->setVisible(false);

    m_animationWidget->setVisible(true);
    m_animationWidget->startAnimation(message);

    qInfo() << "INFO: [GitPullDialog::showProgress] Started progress animation:" << message;
}

void GitPullDialog::hideProgress()
{
    m_isOperationInProgress = false;
    enableControls(true);

    m_progressBar->setVisible(false);
    m_progressLabel->setVisible(false);
    m_animationWidget->setVisible(false);
    m_animationWidget->stopAnimation();

    qInfo() << "INFO: [GitPullDialog::hideProgress] Stopped progress animation";
}

void GitPullDialog::setupUpdatesContextMenu()
{
    m_updatesContextMenu = new QMenu(this);

    // === 浏览器操作 ===
    m_openInBrowserAction = m_updatesContextMenu->addAction(
            QIcon(":/icons/internet-web-browser"), tr("Open in Browser"));
    m_openInBrowserAction->setToolTip(tr("Open commit in web browser"));

    m_updatesContextMenu->addSeparator();

    // === 复制操作 ===
    m_copyHashAction = m_updatesContextMenu->addAction(
            QIcon(":/icons/edit-copy"), tr("Copy Commit Hash"));
    m_copyHashAction->setToolTip(tr("Copy full commit hash to clipboard"));

    m_copyMessageAction = m_updatesContextMenu->addAction(
            QIcon(":/icons/edit-copy"), tr("Copy Commit Message"));
    m_copyMessageAction->setToolTip(tr("Copy commit message to clipboard"));

    m_updatesContextMenu->addSeparator();

    // === 查看操作 ===
    m_showDetailsAction = m_updatesContextMenu->addAction(
            QIcon(":/icons/document-properties"), tr("Show Commit Details"));
    m_showDetailsAction->setToolTip(tr("Show detailed commit information"));

    // === 连接信号 ===
    connect(m_openInBrowserAction, &QAction::triggered, this, &GitPullDialog::openCommitInBrowser);
    connect(m_copyHashAction, &QAction::triggered, this, &GitPullDialog::copyCommitHash);
    connect(m_copyMessageAction, &QAction::triggered, this, &GitPullDialog::copyCommitMessage);
    connect(m_showDetailsAction, &QAction::triggered, this, &GitPullDialog::showCommitDetails);
}

void GitPullDialog::showUpdatesContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_updatesWidget->itemAt(pos);
    if (!item) {
        return;
    }

    // 确保选中了正确的项目
    m_updatesWidget->setCurrentItem(item);

    RemoteUpdateInfo updateInfo = getCurrentSelectedUpdate();
    if (updateInfo.hash.isEmpty()) {
        return;
    }

    // 更新菜单项文本显示commit信息
    QString shortHash = updateInfo.shortHash;
    m_openInBrowserAction->setText(tr("Open %1 in Browser").arg(shortHash));
    m_copyHashAction->setText(tr("Copy Hash (%1)").arg(shortHash));
    m_copyMessageAction->setText(tr("Copy Message"));
    m_showDetailsAction->setText(tr("Show Details for %1").arg(shortHash));

    // 检查是否能够构建浏览器URL
    QString remoteName = m_remoteCombo->currentText();
    QString remoteUrl = getRemoteUrl(remoteName);
    m_openInBrowserAction->setEnabled(!remoteUrl.isEmpty());

    m_updatesContextMenu->exec(m_updatesWidget->mapToGlobal(pos));
}

void GitPullDialog::openCommitInBrowser()
{
    RemoteUpdateInfo updateInfo = getCurrentSelectedUpdate();
    if (updateInfo.hash.isEmpty()) {
        return;
    }

    QString remoteName = m_remoteCombo->currentText();
    QString remoteUrl = getRemoteUrl(remoteName);

    if (remoteUrl.isEmpty()) {
        QMessageBox::warning(this, tr("No Remote URL"),
                             tr("Cannot open commit in browser: no remote URL found for '%1'.")
                                     .arg(remoteName));
        return;
    }

    QString commitUrl = buildCommitUrl(remoteUrl, updateInfo.hash);
    if (commitUrl.isEmpty()) {
        QMessageBox::warning(this, tr("Unsupported Remote"),
                             tr("Cannot open commit in browser: unsupported remote URL format.\n\n"
                                "Remote URL: %1")
                                     .arg(remoteUrl));
        return;
    }

    qInfo() << "INFO: [GitPullDialog::openCommitInBrowser] Opening commit URL:" << commitUrl;

    if (!QDesktopServices::openUrl(QUrl(commitUrl))) {
        QMessageBox::critical(this, tr("Failed to Open Browser"),
                              tr("Failed to open the commit URL in browser.\n\n"
                                 "URL: %1\n\n"
                                 "You can copy this URL manually.")
                                      .arg(commitUrl));
    }
}

void GitPullDialog::copyCommitHash()
{
    RemoteUpdateInfo updateInfo = getCurrentSelectedUpdate();
    if (updateInfo.hash.isEmpty()) {
        return;
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(updateInfo.hash);

    qInfo() << "INFO: [GitPullDialog::copyCommitHash] Copied commit hash to clipboard:" << updateInfo.hash;

    // 显示简短的成功提示
    m_animationWidget->setVisible(true);
    m_animationWidget->getLabel()->setText(tr("✓ Commit hash copied to clipboard"));
    m_animationWidget->getLabel()->setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; font-size: 14px; }");

    // 1秒后隐藏提示
    QTimer::singleShot(1000, this, [this]() {
        m_animationWidget->setVisible(false);
    });
}

void GitPullDialog::copyCommitMessage()
{
    RemoteUpdateInfo updateInfo = getCurrentSelectedUpdate();
    if (updateInfo.message.isEmpty()) {
        return;
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(updateInfo.message);

    qInfo() << "INFO: [GitPullDialog::copyCommitMessage] Copied commit message to clipboard:" << updateInfo.message;

    // 显示简短的成功提示
    m_animationWidget->setVisible(true);
    m_animationWidget->getLabel()->setText(tr("✓ Commit message copied to clipboard"));
    m_animationWidget->getLabel()->setStyleSheet("QLabel { color: #4CAF50; font-weight: bold; font-size: 14px; }");

    // 1秒后隐藏提示
    QTimer::singleShot(1000, this, [this]() {
        m_animationWidget->setVisible(false);
    });
}

void GitPullDialog::showCommitDetails()
{
    RemoteUpdateInfo updateInfo = getCurrentSelectedUpdate();
    if (updateInfo.hash.isEmpty()) {
        return;
    }

    qInfo() << "INFO: [GitPullDialog::showCommitDetails] Showing commit details for:" << updateInfo.hash;

    // 获取详细的提交信息
    GitCommandExecutor executor;
    QString output, error;

    GitCommandExecutor::GitCommand cmd;
    cmd.command = "show";
    cmd.arguments = QStringList() << "show"
                                  << "--stat"
                                  << "--format=fuller"
                                  << updateInfo.hash;
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 10000;

    auto result = executor.executeCommand(cmd, output, error);

    QString detailsText;
    if (result == GitCommandExecutor::Result::Success) {
        detailsText = output;
    } else {
        detailsText = tr("Failed to load commit details: %1").arg(error);
    }

    // 创建详情对话框
    QDialog *detailsDialog = new QDialog(this);
    detailsDialog->setWindowTitle(tr("Commit Details - %1").arg(updateInfo.shortHash));
    detailsDialog->resize(800, 600);
    detailsDialog->setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(detailsDialog);

    // 提交信息标签
    auto *infoLabel = new QLabel(tr("Commit: %1\nMessage: %2")
                                         .arg(updateInfo.hash, updateInfo.message));
    infoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 8px; border: 1px solid #ccc; font-weight: bold; }");
    layout->addWidget(infoLabel);

    // 详细信息文本区域
    auto *detailsTextEdit = new QPlainTextEdit;
    detailsTextEdit->setReadOnly(true);
    detailsTextEdit->setFont(QFont("Consolas", 9));
    detailsTextEdit->setPlainText(detailsText);
    layout->addWidget(detailsTextEdit);

    // 按钮区域
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    auto *copyHashButton = new QPushButton(tr("Copy Hash"));
    connect(copyHashButton, &QPushButton::clicked, [updateInfo]() {
        QApplication::clipboard()->setText(updateInfo.hash);
    });
    buttonLayout->addWidget(copyHashButton);

    auto *closeButton = new QPushButton(tr("Close"));
    connect(closeButton, &QPushButton::clicked, detailsDialog, &QDialog::accept);
    buttonLayout->addWidget(closeButton);

    layout->addLayout(buttonLayout);

    detailsDialog->show();
}

QString GitPullDialog::getRemoteUrl(const QString &remoteName) const
{
    if (remoteName.isEmpty()) {
        return QString();
    }

    GitCommandExecutor executor;
    QString output, error;

    GitCommandExecutor::GitCommand cmd;
    cmd.command = "remote";
    cmd.arguments = QStringList() << "remote"
                                  << "get-url"
                                  << remoteName;
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);

    if (result == GitCommandExecutor::Result::Success) {
        QString url = output.trimmed();
        qInfo() << "INFO: [GitPullDialog::getRemoteUrl] Got remote URL for" << remoteName << ":" << url;
        return url;
    } else {
        qWarning() << "WARNING: [GitPullDialog::getRemoteUrl] Failed to get remote URL for" << remoteName << ":" << error;
        return QString();
    }
}

QString GitPullDialog::buildCommitUrl(const QString &remoteUrl, const QString &commitHash) const
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

GitPullDialog::RemoteUpdateInfo GitPullDialog::getCurrentSelectedUpdate() const
{
    QListWidgetItem *currentItem = m_updatesWidget->currentItem();
    if (!currentItem) {
        return RemoteUpdateInfo();
    }

    int currentRow = m_updatesWidget->row(currentItem);
    if (currentRow < 0 || currentRow >= m_remoteUpdates.size()) {
        return RemoteUpdateInfo();
    }

    return m_remoteUpdates[currentRow];
}

void GitPullDialog::onItemDoubleClicked(QListWidgetItem *item)
{
    if (!item) {
        return;
    }

    RemoteUpdateInfo updateInfo = getCurrentSelectedUpdate();
    if (updateInfo.hash.isEmpty()) {
        return;
    }

    // 打开浏览器
    openCommitInBrowser();
}
