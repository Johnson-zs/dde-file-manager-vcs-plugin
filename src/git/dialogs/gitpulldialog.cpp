#include "gitpulldialog.h"
#include "../gitoperationservice.h"
#include "../gitcommandexecutor.h"
#include "../utils.h"
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

GitPullDialog::GitPullDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_operationService(new GitOperationService(this)), m_hasLocalChanges(false), m_hasUncommittedChanges(false), m_isOperationInProgress(false), m_isDryRunInProgress(false), m_statusUpdateTimer(new QTimer(this))
{
    setWindowTitle(tr("Git Pull"));
    setWindowIcon(QIcon(":/icons/vcs-pull"));
    setMinimumSize(800, 500);
    resize(900, 600);

    qInfo() << "INFO: [GitPullDialog] Initializing pull dialog for repository:" << repositoryPath;

    setupUI();
    setupConnections();
    loadRepositoryInfo();

    // 设置定时器更新状态
    m_statusUpdateTimer->setSingleShot(false);
    m_statusUpdateTimer->setInterval(30000);   // 30秒更新一次
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &GitPullDialog::refreshRemoteUpdates);
    m_statusUpdateTimer->start();
}

GitPullDialog::~GitPullDialog()
{
    qInfo() << "INFO: [GitPullDialog] Destroying pull dialog";
}

void GitPullDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);

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

    // 进度条
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    m_progressLabel = new QLabel;
    m_progressLabel->setVisible(false);

    mainLayout->addWidget(m_progressLabel);
    mainLayout->addWidget(m_progressBar);

    setupButtonGroup();

    // 修复布局添加问题
    auto *buttonWidget = new QWidget;
    auto *buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setSpacing(6);
    buttonLayout->setContentsMargins(0, 0, 0, 0);

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
    m_pullButton->setStyleSheet("QPushButton { font-weight: bold; padding: 6px 12px; }");

    m_cancelButton = new QPushButton(tr("Cancel"));
    m_cancelButton->setIcon(QIcon(":/icons/dialog-cancel"));

    buttonLayout->addWidget(m_pullButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addWidget(buttonWidget);
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
        m_remotes = output.split('\n', Qt::SkipEmptyParts);
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
    cmd.arguments = QStringList() << "branch"
                                  << "--show-current";
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
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
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
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);

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
    // 简化实现：显示占位信息
    m_remoteUpdates.clear();
    m_updatesWidget->clear();

    m_updatesCountLabel->setText(tr("Click 'Fetch Updates' to check for remote changes"));
    m_downloadStatsLabel->setText(tr("Remote status: Unknown"));

    auto *item = new QListWidgetItem(tr("Fetch updates to see available changes"));
    item->setIcon(QIcon(":/icons/vcs-update-required"));
    m_updatesWidget->addItem(item);
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
    qInfo() << "INFO: [GitPullDialog::fetchUpdates] Fetching updates";

    // TODO: 实现fetch功能
    QMessageBox::information(this, tr("Fetch Updates"),
                             tr("Fetch functionality will be implemented in the next phase."));
}

void GitPullDialog::stashAndPull()
{
    qInfo() << "INFO: [GitPullDialog::stashAndPull] Stash and pull";

    if (!m_hasLocalChanges) {
        QMessageBox::information(this, tr("No Changes"),
                                 tr("No local changes to stash. Proceeding with normal pull."));
        executePull();
        return;
    }

    // TODO: 实现stash and pull功能
    QMessageBox::information(this, tr("Stash & Pull"),
                             tr("Stash & Pull functionality will be implemented in the next phase."));
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
    m_isOperationInProgress = true;
    m_isDryRunInProgress = options.dryRun;
    enableControls(false);

    m_progressBar->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progressBar->setRange(0, 0);   // 不确定进度

    if (options.dryRun) {
        m_progressLabel->setText(tr("Running pull dry run..."));
    } else {
        m_progressLabel->setText(tr("Pulling from remote repository..."));
    }

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
    
    m_isOperationInProgress = false;
    m_isDryRunInProgress = false;
    enableControls(true);
    
    m_progressBar->setVisible(false);
    m_progressLabel->setVisible(false);
    
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
            m_progressLabel->setText(tr("Pull completed successfully!"));
            m_progressLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
            m_progressLabel->setVisible(true);
            
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
    // TODO: 实现冲突处理
    QMessageBox::information(this, tr("Handle Conflicts"),
                             tr("Conflict handling functionality will be implemented in the next phase."));
}

void GitPullDialog::updateLocalStatus()
{
    validatePullOptions();
}

void GitPullDialog::refreshRemoteUpdates()
{
    if (!m_isOperationInProgress) {
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
