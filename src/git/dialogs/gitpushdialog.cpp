#include "gitpushdialog.h"
#include "gitoperationservice.h"
#include "gitcommandexecutor.h"
#include "gitdialogs.h"
#include "widgets/linenumbertextedit.h"

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
#include <QTextEdit>
#include <QHeaderView>
#include <QFileInfo>
#include <QDir>
#include <QFont>
#include <QMenu>
#include <QAction>

GitPushDialog::GitPushDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_operationService(new GitOperationService(this)), m_isOperationInProgress(false), m_isDryRunInProgress(false), m_statusUpdateTimer(new QTimer(this))
{
    setWindowTitle(tr("Git Push"));
    setWindowIcon(QIcon(":/icons/vcs-push"));
    setMinimumSize(800, 500);
    resize(900, 600);

    qInfo() << "INFO: [GitPushDialog] Initializing push dialog for repository:" << repositoryPath;

    setupUI();
    setupConnections();
    loadRepositoryInfo();

    // 设置定时器更新状态
    m_statusUpdateTimer->setSingleShot(false);
    m_statusUpdateTimer->setInterval(30000);   // 30秒更新一次
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &GitPushDialog::refreshRemoteStatus);
    m_statusUpdateTimer->start();
}

GitPushDialog::~GitPushDialog()
{
    qInfo() << "INFO: [GitPushDialog] Destroying push dialog";
}

void GitPushDialog::setupUI()
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

    setupCommitsGroup();
    rightLayout->addWidget(m_commitsGroup);

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
    buttonWidget->setFixedHeight(50);   // 减少容器高度，让按钮有更多空间
    auto *buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setSpacing(6);
    buttonLayout->setContentsMargins(0, 8, 0, 8);   // 增加上下边距，让按钮居中

    // 远程管理按钮
    m_remoteManagerButton = new QPushButton(tr("Remote Manager"));
    m_remoteManagerButton->setIcon(QIcon(":/icons/vcs-branch"));
    m_remoteManagerButton->setToolTip(tr("Manage remote repositories"));

    // 预览按钮
    m_previewButton = new QPushButton(tr("Preview Changes"));
    m_previewButton->setIcon(QIcon(":/icons/vcs-diff"));
    m_previewButton->setToolTip(tr("Preview what will be pushed"));

    // 试运行按钮
    m_dryRunButton = new QPushButton(tr("Dry Run"));
    m_dryRunButton->setIcon(QIcon(":/icons/vcs-status"));
    m_dryRunButton->setToolTip(tr("Test push without actually pushing"));

    // 影响评估按钮
    auto *impactButton = new QPushButton(tr("Impact Assessment"));
    impactButton->setIcon(QIcon(":/icons/vcs-status"));
    impactButton->setToolTip(tr("Analyze the impact of this push operation"));
    connect(impactButton, &QPushButton::clicked, this, &GitPushDialog::showImpactAssessment);

    buttonLayout->addWidget(m_remoteManagerButton);
    buttonLayout->addWidget(m_previewButton);
    buttonLayout->addWidget(m_dryRunButton);
    buttonLayout->addWidget(impactButton);
    buttonLayout->addStretch();

    // 主要操作按钮
    m_pushButton = new QPushButton(tr("Push"));
    m_pushButton->setIcon(QIcon(":/icons/vcs-push"));
    m_pushButton->setDefault(true);
    m_pushButton->setStyleSheet("QPushButton { font-weight: bold; }");   // 移除自定义padding

    m_cancelButton = new QPushButton(tr("Cancel"));
    m_cancelButton->setIcon(QIcon(":/icons/dialog-cancel"));

    buttonLayout->addWidget(m_pushButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addWidget(buttonWidget);
}

void GitPushDialog::setupStatusGroup()
{
    m_statusGroup = new QGroupBox(tr("Repository Status"));
    auto *layout = new QGridLayout(m_statusGroup);
    layout->setSpacing(8);

    // 当前分支
    layout->addWidget(new QLabel(tr("Current Branch:")), 0, 0);
    m_currentBranchLabel = new QLabel(tr("Loading..."));
    m_currentBranchLabel->setStyleSheet("font-weight: bold; color: #2196F3;");
    layout->addWidget(m_currentBranchLabel, 0, 1);

    // 未推送提交数
    layout->addWidget(new QLabel(tr("Unpushed Commits:")), 1, 0);
    m_unpushedCountLabel = new QLabel(tr("Loading..."));
    m_unpushedCountLabel->setStyleSheet("font-weight: bold; color: #FF9800;");
    layout->addWidget(m_unpushedCountLabel, 1, 1);

    // 远程状态
    layout->addWidget(new QLabel(tr("Remote Status:")), 2, 0);
    m_remoteStatusLabel = new QLabel(tr("Checking..."));
    layout->addWidget(m_remoteStatusLabel, 2, 1);

    // 最后推送时间
    layout->addWidget(new QLabel(tr("Last Push:")), 3, 0);
    m_lastPushLabel = new QLabel(tr("Unknown"));
    layout->addWidget(m_lastPushLabel, 3, 1);

    layout->setColumnStretch(1, 1);
}

void GitPushDialog::setupConfigGroup()
{
    m_configGroup = new QGroupBox(tr("Push Configuration"));
    auto *layout = new QGridLayout(m_configGroup);
    layout->setSpacing(8);

    // 远程仓库选择
    layout->addWidget(new QLabel(tr("Remote Repository:")), 0, 0);
    m_remoteCombo = new QComboBox;
    m_remoteCombo->setMinimumWidth(200);
    layout->addWidget(m_remoteCombo, 0, 1);

    // 本地分支
    layout->addWidget(new QLabel(tr("Local Branch:")), 1, 0);
    m_localBranchCombo = new QComboBox;
    layout->addWidget(m_localBranchCombo, 1, 1);

    // 远程分支
    layout->addWidget(new QLabel(tr("Remote Branch:")), 2, 0);
    m_remoteBranchCombo = new QComboBox;
    m_remoteBranchCombo->setEditable(true);
    layout->addWidget(m_remoteBranchCombo, 2, 1);

    // 推送选项
    m_forceCheckBox = new QCheckBox(tr("Force push (--force-with-lease)"));
    m_forceCheckBox->setToolTip(tr("Safely force push, preventing accidental overwrites"));
    layout->addWidget(m_forceCheckBox, 3, 0, 1, 2);

    m_tagsCheckBox = new QCheckBox(tr("Push tags (--tags)"));
    m_tagsCheckBox->setToolTip(tr("Push all local tags to remote repository"));
    layout->addWidget(m_tagsCheckBox, 4, 0, 1, 2);

    m_upstreamCheckBox = new QCheckBox(tr("Set upstream branch (-u)"));
    m_upstreamCheckBox->setToolTip(tr("Set the remote branch as upstream for the local branch"));
    layout->addWidget(m_upstreamCheckBox, 5, 0, 1, 2);

    m_allBranchesCheckBox = new QCheckBox(tr("Push all branches (--all)"));
    m_allBranchesCheckBox->setToolTip(tr("Push all local branches to remote repository"));
    layout->addWidget(m_allBranchesCheckBox, 6, 0, 1, 2);

    layout->setColumnStretch(1, 1);
}

void GitPushDialog::setupCommitsGroup()
{
    m_commitsGroup = new QGroupBox(tr("Commits to Push"));
    auto *layout = new QVBoxLayout(m_commitsGroup);
    layout->setSpacing(8);

    // 提交数量标签
    m_commitsCountLabel = new QLabel(tr("Loading commits..."));
    m_commitsCountLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_commitsCountLabel);

    // 提交列表
    m_commitsWidget = new QListWidget;
    m_commitsWidget->setAlternatingRowColors(true);
    m_commitsWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(m_commitsWidget);
}

void GitPushDialog::setupButtonGroup()
{
    // 按钮创建已移动到setupUI方法中，这里只保留空实现
}

void GitPushDialog::setupConnections()
{
    // 组合框信号
    connect(m_remoteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GitPushDialog::onRemoteChanged);
    connect(m_localBranchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GitPushDialog::onBranchChanged);

    // 复选框信号
    connect(m_forceCheckBox, &QCheckBox::toggled, this, &GitPushDialog::onForceToggled);
    connect(m_tagsCheckBox, &QCheckBox::toggled, this, &GitPushDialog::onTagsToggled);
    connect(m_upstreamCheckBox, &QCheckBox::toggled, this, &GitPushDialog::onUpstreamToggled);

    // 按钮信号
    connect(m_remoteManagerButton, &QPushButton::clicked, this, &GitPushDialog::showRemoteManager);
    connect(m_previewButton, &QPushButton::clicked, this, [this]() {
        // 创建预览选择菜单
        QMenu *previewMenu = new QMenu(this);
        
        auto *branchCompareAction = previewMenu->addAction(QIcon(":/icons/vcs-branch"), tr("Branch Comparison Preview"));
        branchCompareAction->setToolTip(tr("Use advanced branch comparison dialog"));
        connect(branchCompareAction, &QAction::triggered, this, &GitPushDialog::previewChanges);
        
        auto *quickPreviewAction = previewMenu->addAction(QIcon(":/icons/vcs-diff"), tr("Quick Preview"));
        quickPreviewAction->setToolTip(tr("Show simple preview with commit list and file changes"));
        connect(quickPreviewAction, &QAction::triggered, this, &GitPushDialog::showQuickPreview);
        
        previewMenu->exec(m_previewButton->mapToGlobal(QPoint(0, m_previewButton->height())));
    });
    connect(m_dryRunButton, &QPushButton::clicked, this, &GitPushDialog::executeDryRun);
    connect(m_pushButton, &QPushButton::clicked, this, &GitPushDialog::executePush);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    // 操作服务信号 - 修复参数匹配问题
    connect(m_operationService, &GitOperationService::operationCompleted,
            this, [this](const QString &operation, bool success, const QString &message) {
                Q_UNUSED(operation)
                onPushCompleted(success, message);
            });
}

void GitPushDialog::loadRepositoryInfo()
{
    qInfo() << "INFO: [GitPushDialog::loadRepositoryInfo] Loading repository information";

    loadRemotes();
    loadBranches();
    loadUnpushedCommits();
    loadRemoteStatus();
    updateRepositoryStatus();
}

void GitPushDialog::loadRemotes()
{
    GitCommandExecutor executor;
    QString output, error;

    GitCommandExecutor::GitCommand cmd;
    cmd.command = "remote";
    cmd.arguments = QStringList() << "remote"
                                  << "-v";
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);

    m_remotes.clear();
    m_remoteCombo->clear();

    if (result == GitCommandExecutor::Result::Success) {
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        QSet<QString> remoteNames;

        for (const QString &line : lines) {
            QStringList parts = line.split('\t');
            if (parts.size() >= 2) {
                QString remoteName = parts[0];
                if (!remoteNames.contains(remoteName)) {
                    remoteNames.insert(remoteName);
                    m_remotes.append(remoteName);
                }
            }
        }

        m_remoteCombo->addItems(m_remotes);

        // 默认选择origin
        int originIndex = m_remotes.indexOf("origin");
        if (originIndex >= 0) {
            m_remoteCombo->setCurrentIndex(originIndex);
        }

        qInfo() << "INFO: [GitPushDialog::loadRemotes] Loaded" << m_remotes.size() << "remotes";
    } else {
        qWarning() << "WARNING: [GitPushDialog::loadRemotes] Failed to load remotes:" << error;
        m_remoteCombo->addItem(tr("No remotes found"));
        m_remoteCombo->setEnabled(false);
    }
}

void GitPushDialog::loadBranches()
{
    GitCommandExecutor executor;
    QString output, error;

    // 加载本地分支
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "branch";
    cmd.arguments = QStringList() << "branch";
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);

    m_localBranches.clear();
    m_localBranchCombo->clear();

    if (result == GitCommandExecutor::Result::Success) {
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            QString branchName = line.trimmed();
            if (branchName.startsWith("* ")) {
                branchName = branchName.mid(2);
                m_currentBranch = branchName;
            }
            m_localBranches.append(branchName);
        }

        m_localBranchCombo->addItems(m_localBranches);

        // 设置当前分支
        int currentIndex = m_localBranches.indexOf(m_currentBranch);
        if (currentIndex >= 0) {
            m_localBranchCombo->setCurrentIndex(currentIndex);
        }

        m_currentBranchLabel->setText(m_currentBranch);

        qInfo() << "INFO: [GitPushDialog::loadBranches] Current branch:" << m_currentBranch;
    } else {
        qWarning() << "WARNING: [GitPushDialog::loadBranches] Failed to load branches:" << error;
    }

    // 加载远程分支
    loadRemoteBranches();
}

void GitPushDialog::loadRemoteBranches()
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
        } else if (!m_remoteBranches.isEmpty()) {
            m_remoteBranchCombo->setCurrentText(m_currentBranch);
        }

        qInfo() << "INFO: [GitPushDialog::loadRemoteBranches] Loaded" << m_remoteBranches.size() << "remote branches";
    }
}

void GitPushDialog::loadUnpushedCommits()
{
    if (m_remoteCombo->currentText().isEmpty() || m_currentBranch.isEmpty()) {
        return;
    }

    GitCommandExecutor executor;
    QString output, error;

    QString remoteBranch = m_remoteCombo->currentText() + "/" + m_currentBranch;

    GitCommandExecutor::GitCommand cmd;
    cmd.command = "log";
    cmd.arguments = QStringList() << "log"
                                  << "--oneline"
                                  << "--no-merges"
                                  << (remoteBranch + ".." + m_currentBranch);
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 10000;

    auto result = executor.executeCommand(cmd, output, error);

    m_unpushedCommits.clear();
    m_commitsWidget->clear();

    if (result == GitCommandExecutor::Result::Success && !output.trimmed().isEmpty()) {
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            CommitInfo commit;
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (!parts.isEmpty()) {
                commit.shortHash = parts[0];
                commit.hash = parts[0];   // 简化处理
                commit.message = parts.mid(1).join(' ');
                commit.author = tr("Unknown");   // 需要额外查询
                commit.timestamp = QDateTime::currentDateTime();   // 需要额外查询

                m_unpushedCommits.append(commit);

                // 添加到列表
                auto *item = new QListWidgetItem(formatCommitInfo(commit));
                item->setIcon(QIcon(":/icons/vcs-commit"));
                m_commitsWidget->addItem(item);
            }
        }

        m_unpushedCountLabel->setText(QString::number(m_unpushedCommits.size()));
        m_commitsCountLabel->setText(tr("%1 commits to push").arg(m_unpushedCommits.size()));

        qInfo() << "INFO: [GitPushDialog::loadUnpushedCommits] Found" << m_unpushedCommits.size() << "unpushed commits";
    } else {
        m_unpushedCountLabel->setText("0");
        m_commitsCountLabel->setText(tr("No commits to push"));

        auto *item = new QListWidgetItem(tr("No unpushed commits found"));
        item->setIcon(QIcon(":/icons/vcs-normal"));
        m_commitsWidget->addItem(item);

        qInfo() << "INFO: [GitPushDialog::loadUnpushedCommits] No unpushed commits found";
    }
}

void GitPushDialog::updateRepositoryStatus()
{
    // 更新状态标签
    updateUI();
}

void GitPushDialog::updateStatusLabels()
{
    // 更新远程状态
    if (m_unpushedCommits.isEmpty()) {
        m_remoteStatusLabel->setText(tr("Up to date"));
        m_remoteStatusLabel->setStyleSheet("color: #4CAF50;");
    } else {
        m_remoteStatusLabel->setText(tr("Behind by %1 commits").arg(m_unpushedCommits.size()));
        m_remoteStatusLabel->setStyleSheet("color: #FF9800;");
    }
}

void GitPushDialog::validatePushOptions()
{
    bool canPush = !m_remoteCombo->currentText().isEmpty() && !m_localBranchCombo->currentText().isEmpty() && !m_isOperationInProgress;

    m_pushButton->setEnabled(canPush);
    m_dryRunButton->setEnabled(canPush);
    m_previewButton->setEnabled(canPush);
}

QString GitPushDialog::formatCommitInfo(const CommitInfo &commit) const
{
    return QString("● %1 %2").arg(commit.shortHash, commit.message);
}

// 槽函数实现
void GitPushDialog::onRemoteChanged()
{
    qInfo() << "INFO: [GitPushDialog::onRemoteChanged] Remote changed to:" << m_remoteCombo->currentText();
    loadRemoteBranches();
    loadUnpushedCommits();
    updateRepositoryStatus();
}

void GitPushDialog::onBranchChanged()
{
    qInfo() << "INFO: [GitPushDialog::onBranchChanged] Branch changed to:" << m_localBranchCombo->currentText();
    loadUnpushedCommits();
    updateRepositoryStatus();
}

void GitPushDialog::onForceToggled(bool enabled)
{
    if (enabled) {
        QMessageBox::warning(this, tr("Force Push Warning"),
                             tr("Force push can overwrite remote changes and cause data loss.\n"
                                "Only use this if you are certain about what you're doing.\n\n"
                                "Consider using 'git pull' first to merge remote changes."));
        
        // 显示影响评估
        showImpactAssessment();
    }
}

void GitPushDialog::onTagsToggled(bool enabled)
{
    Q_UNUSED(enabled)
    // 标签推送的额外逻辑可以在这里添加
}

void GitPushDialog::onUpstreamToggled(bool enabled)
{
    Q_UNUSED(enabled)
    // 上游设置的额外逻辑可以在这里添加
}

void GitPushDialog::showRemoteManager()
{
    qInfo() << "INFO: [GitPushDialog::showRemoteManager] Opening remote manager";

    GitDialogManager::instance()->showRemoteManager(m_repositoryPath, this);
}

void GitPushDialog::previewChanges()
{
    qInfo() << "INFO: [GitPushDialog::previewChanges] Starting changes preview";

    if (m_unpushedCommits.isEmpty()) {
        QMessageBox::information(this, tr("No Changes to Preview"),
                                 tr("There are no unpushed commits to preview."));
        return;
    }

    // 使用GitBranchComparisonDialog来预览推送内容
    QString remoteBranch = m_remoteCombo->currentText() + "/" + m_remoteBranchCombo->currentText();
    QString localBranch = m_localBranchCombo->currentText();
    
    // 调用GitDialogManager来显示分支比较，比较远程分支和本地分支的差异
    GitDialogManager::instance()->showBranchComparisonDialog(m_repositoryPath, remoteBranch, localBranch, this);
}

void GitPushDialog::executeDryRun()
{
    qInfo() << "INFO: [GitPushDialog::executeDryRun] Starting dry run";

    PushOptions options;
    options.remoteName = m_remoteCombo->currentText();
    options.localBranch = m_localBranchCombo->currentText();
    options.remoteBranch = m_remoteBranchCombo->currentText();
    options.forceWithLease = m_forceCheckBox->isChecked();
    options.pushTags = m_tagsCheckBox->isChecked();
    options.setUpstream = m_upstreamCheckBox->isChecked();
    options.pushAllBranches = m_allBranchesCheckBox->isChecked();
    options.dryRun = true;

    executePushWithOptions(options);
}

void GitPushDialog::executePush()
{
    qInfo() << "INFO: [GitPushDialog::executePush] Starting push operation";

    // 强制推送确认
    if (m_forceCheckBox->isChecked() && !confirmForcePush()) {
        return;
    }

    PushOptions options;
    options.remoteName = m_remoteCombo->currentText();
    options.localBranch = m_localBranchCombo->currentText();
    options.remoteBranch = m_remoteBranchCombo->currentText();
    options.forceWithLease = m_forceCheckBox->isChecked();
    options.pushTags = m_tagsCheckBox->isChecked();
    options.setUpstream = m_upstreamCheckBox->isChecked();
    options.pushAllBranches = m_allBranchesCheckBox->isChecked();
    options.dryRun = false;

    executePushWithOptions(options);
}

void GitPushDialog::executePushWithOptions(const PushOptions &options)
{
    m_isOperationInProgress = true;
    m_isDryRunInProgress = options.dryRun;
    enableControls(false);

    m_progressBar->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progressBar->setRange(0, 0);   // 不确定进度

    if (options.dryRun) {
        m_progressLabel->setText(tr("Running dry run..."));
    } else {
        m_progressLabel->setText(tr("Pushing to remote repository..."));
    }

    qInfo() << "INFO: [GitPushDialog::executePushWithOptions] Executing push with options:"
            << "remote:" << options.remoteName << "local:" << options.localBranch
            << "remote branch:" << options.remoteBranch << "force:" << options.forceWithLease
            << "tags:" << options.pushTags << "upstream:" << options.setUpstream
            << "all branches:" << options.pushAllBranches << "dry-run:" << options.dryRun;

    // 使用GitOperationService执行推送
    if (options.pushAllBranches) {
        // 推送所有分支的特殊处理
        m_operationService->pushWithOptions(m_repositoryPath, options.remoteName,
                                            "--all", "", options.forceWithLease,
                                            options.pushTags, false, options.dryRun);
    } else {
        m_operationService->pushWithOptions(m_repositoryPath, options.remoteName,
                                            options.localBranch, options.remoteBranch,
                                            options.forceWithLease, options.pushTags,
                                            options.setUpstream, options.dryRun);
    }
}

bool GitPushDialog::confirmForcePush()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm Force Push"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(tr("You are about to force push to the remote repository."));
    msgBox.setInformativeText(tr("This operation can overwrite remote changes and cause data loss.\n"
                                 "Are you sure you want to continue?"));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    return msgBox.exec() == QMessageBox::Yes;
}

void GitPushDialog::onPushCompleted(bool success, const QString &message)
{
    bool isDryRun = m_isDryRunInProgress;

    m_isOperationInProgress = false;
    m_isDryRunInProgress = false;
    enableControls(true);

    m_progressBar->setVisible(false);
    m_progressLabel->setVisible(false);

    if (success) {
        qInfo() << "INFO: [GitPushDialog::onPushCompleted] Push completed successfully";

        if (isDryRun) {
            QMessageBox::information(this, tr("Dry Run Successful"),
                                     tr("Dry run completed successfully. No changes were made.\n\n%1").arg(message));
        } else {
            // 刷新状态（仅在实际push后）
            loadUnpushedCommits();
            updateRepositoryStatus();

            // 关闭对话框
            accept();
        }
    } else {
        qWarning() << "WARNING: [GitPushDialog::onPushCompleted] Push failed:" << message;

        if (isDryRun) {
            QMessageBox::critical(this, tr("Dry Run Failed"),
                                  tr("Dry run failed.\n\n%1").arg(message));
        } else {
            QMessageBox::critical(this, tr("Push Failed"),
                                  tr("Push operation failed.\n\n%1").arg(message));
        }
    }
}

void GitPushDialog::refreshRemoteStatus()
{
    if (!m_isOperationInProgress) {
        qInfo() << "INFO: [GitPushDialog::refreshRemoteStatus] Refreshing remote status";
        loadUnpushedCommits();
        updateRepositoryStatus();
    }
}

void GitPushDialog::enableControls(bool enabled)
{
    m_remoteCombo->setEnabled(enabled);
    m_localBranchCombo->setEnabled(enabled);
    m_remoteBranchCombo->setEnabled(enabled);
    m_forceCheckBox->setEnabled(enabled);
    m_tagsCheckBox->setEnabled(enabled);
    m_upstreamCheckBox->setEnabled(enabled);
    m_allBranchesCheckBox->setEnabled(enabled);
    m_remoteManagerButton->setEnabled(enabled);
    m_previewButton->setEnabled(enabled);
    m_dryRunButton->setEnabled(enabled);
    m_pushButton->setEnabled(enabled);

    // 在启用控件后，重新验证按钮状态
    if (enabled) {
        validatePushOptions();
    }
}

QString GitPushDialog::getFileChangesPreview() const
{
    qInfo() << "INFO: [GitPushDialog::getFileChangesPreview] Generating file changes preview";

    if (m_unpushedCommits.isEmpty()) {
        return tr("No commits to preview.");
    }

    GitCommandExecutor executor;
    QString output, error;
    QString remoteBranch = m_remoteCombo->currentText() + "/" + m_currentBranch;

    // 获取文件变更统计
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "diff";
    cmd.arguments = QStringList() << "diff"
                                  << "--stat"
                                  << remoteBranch + ".." + m_currentBranch;
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 10000;

    auto result = executor.executeCommand(cmd, output, error);

    if (result == GitCommandExecutor::Result::Success && !output.trimmed().isEmpty()) {
        return output;
    } else {
        // 如果没有远程分支比较，显示所有未推送提交的文件变更
        QString allChanges;
        for (const auto &commit : m_unpushedCommits) {
            GitCommandExecutor::GitCommand commitCmd;
            commitCmd.command = "show";
            commitCmd.arguments = QStringList() << "show"
                                                << "--stat"
                                                << "--format=format:Commit: %h - %s"
                                                << commit.hash;
            commitCmd.workingDirectory = m_repositoryPath;
            commitCmd.timeout = 5000;

            QString commitOutput, commitError;
            auto commitResult = executor.executeCommand(commitCmd, commitOutput, commitError);

            if (commitResult == GitCommandExecutor::Result::Success) {
                allChanges += commitOutput + "\n\n";
            }
        }

        return allChanges.isEmpty() ? tr("Unable to generate file changes preview.") : allChanges;
    }
}

void GitPushDialog::loadRemoteStatus()
{
    qInfo() << "INFO: [GitPushDialog::loadRemoteStatus] Loading remote status";

    if (m_remoteCombo->currentText().isEmpty()) {
        m_lastPushLabel->setText(tr("No remote selected"));
        return;
    }

    GitCommandExecutor executor;
    QString output, error;

    // 获取最后一次推送的信息（通过reflog）
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "reflog";
    cmd.arguments = QStringList() << "reflog"
                                  << "--grep=push"
                                  << "--format=%cd"
                                  << "--date=relative"
                                  << "-1";
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);

    if (result == GitCommandExecutor::Result::Success && !output.trimmed().isEmpty()) {
        m_lastPushLabel->setText(output.trimmed());
    } else {
        m_lastPushLabel->setText(tr("Never"));
    }
}

bool GitPushDialog::checkRemoteStatus()
{
    qInfo() << "INFO: [GitPushDialog::checkRemoteStatus] Checking remote status";

    if (m_remoteCombo->currentText().isEmpty()) {
        return false;
    }

    // 使用GitOperationService测试远程连接
    return m_operationService->testRemoteConnection(m_repositoryPath, m_remoteCombo->currentText());
}

void GitPushDialog::showImpactAssessment()
{
    qInfo() << "INFO: [GitPushDialog::showImpactAssessment] Showing impact assessment";

    if (m_unpushedCommits.isEmpty()) {
        QMessageBox::information(this, tr("No Impact"),
                                 tr("There are no unpushed commits. No impact on remote repository."));
        return;
    }

    // 创建影响评估对话框
    auto *assessmentDialog = new QDialog(this);
    assessmentDialog->setWindowTitle(tr("Push Impact Assessment"));
    assessmentDialog->setWindowIcon(QIcon(":/icons/vcs-status"));
    assessmentDialog->setMinimumSize(600, 400);
    assessmentDialog->resize(700, 500);
    assessmentDialog->setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(assessmentDialog);
    layout->setSpacing(8);
    layout->setContentsMargins(12, 12, 12, 12);

    // 标题
    auto *titleLabel = new QLabel(tr("Impact Assessment for Push Operation"));
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #FF9800;");
    layout->addWidget(titleLabel);

    // 影响信息
    auto *infoText = new QTextEdit;
    infoText->setReadOnly(true);

    QString impactInfo;
    impactInfo += tr("Target: %1/%2\n").arg(m_remoteCombo->currentText(), m_remoteBranchCombo->currentText());
    impactInfo += tr("Local Branch: %1\n").arg(m_localBranchCombo->currentText());
    impactInfo += tr("Commits to Push: %1\n\n").arg(m_unpushedCommits.size());

    impactInfo += tr("Potential Impact:\n");
    impactInfo += tr("• %1 new commits will be added to remote branch\n").arg(m_unpushedCommits.size());

    if (m_forceCheckBox->isChecked()) {
        impactInfo += tr("• ⚠️  FORCE PUSH: May overwrite remote changes\n");
        impactInfo += tr("• ⚠️  Risk of data loss if others have pushed changes\n");
    }

    if (m_tagsCheckBox->isChecked()) {
        impactInfo += tr("• Local tags will be pushed to remote\n");
    }

    if (m_upstreamCheckBox->isChecked()) {
        impactInfo += tr("• Upstream tracking will be set for local branch\n");
    }

    if (m_allBranchesCheckBox->isChecked()) {
        impactInfo += tr("• ⚠️  ALL local branches will be pushed\n");
    }

    impactInfo += tr("\nRecommendations:\n");
    if (m_forceCheckBox->isChecked()) {
        impactInfo += tr("• Consider using 'git pull' first to merge remote changes\n");
        impactInfo += tr("• Verify that no one else is working on the same branch\n");
    } else {
        impactInfo += tr("• This is a safe push operation\n");
        impactInfo += tr("• No risk of overwriting remote changes\n");
    }

    infoText->setPlainText(impactInfo);
    layout->addWidget(infoText);

    // 按钮
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    auto *closeButton = new QPushButton(tr("Close"));
    closeButton->setIcon(QIcon(":/icons/dialog-close"));
    connect(closeButton, &QPushButton::clicked, assessmentDialog, &QDialog::accept);

    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    assessmentDialog->show();
}

void GitPushDialog::showQuickPreview()
{
    qInfo() << "INFO: [GitPushDialog::showQuickPreview] Showing quick push preview";

    if (m_unpushedCommits.isEmpty()) {
        QMessageBox::information(this, tr("No Changes to Preview"),
                                 tr("There are no unpushed commits to preview."));
        return;
    }

    // 创建简洁的预览对话框
    auto *previewDialog = new QDialog(this);
    previewDialog->setWindowTitle(tr("Quick Push Preview"));
    previewDialog->setWindowIcon(QIcon(":/icons/vcs-diff"));
    previewDialog->setMinimumSize(700, 500);
    previewDialog->resize(800, 600);
    previewDialog->setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(previewDialog);
    layout->setSpacing(8);
    layout->setContentsMargins(12, 12, 12, 12);

    // 预览信息
    QString previewInfo = tr("Push Target: %1 → %2/%3\n")
                                 .arg(m_localBranchCombo->currentText())
                                 .arg(m_remoteCombo->currentText())
                                 .arg(m_remoteBranchCombo->currentText());
    previewInfo += tr("Commits to Push: %1\n\n").arg(m_unpushedCommits.size());

    // 提交列表
    previewInfo += tr("Commits:\n");
    for (const auto &commit : m_unpushedCommits) {
        previewInfo += tr("• %1 %2\n").arg(commit.shortHash, commit.message);
    }

    // 文件变更统计
    QString fileChanges = getFileChangesPreview();
    if (!fileChanges.isEmpty()) {
        previewInfo += tr("\nFile Changes:\n") + fileChanges;
    }

    // 使用带行号的文本编辑器显示（如果可用）
    auto *textEdit = new LineNumberTextEdit;
    textEdit->setReadOnly(true);
    textEdit->setFont(QFont("Consolas", 9));
    textEdit->setPlainText(previewInfo);
    layout->addWidget(textEdit);

    // 按钮区域
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    auto *closeButton = new QPushButton(tr("Close"));
    closeButton->setIcon(QIcon(":/icons/dialog-close"));
    connect(closeButton, &QPushButton::clicked, previewDialog, &QDialog::accept);

    auto *pushButton = new QPushButton(tr("Push Now"));
    pushButton->setIcon(QIcon(":/icons/vcs-push"));
    pushButton->setDefault(true);
    pushButton->setStyleSheet("QPushButton { font-weight: bold; }");
    connect(pushButton, &QPushButton::clicked, [this, previewDialog]() {
        previewDialog->accept();
        executePush();
    });

    buttonLayout->addWidget(closeButton);
    buttonLayout->addWidget(pushButton);
    layout->addLayout(buttonLayout);

    previewDialog->show();
}

void GitPushDialog::updateUI()
{
    qInfo() << "INFO: [GitPushDialog::updateUI] Updating UI state";

    updateStatusLabels();
    updateCommitsList();
    validatePushOptions();
    loadRemoteStatus();
}

void GitPushDialog::updateCommitsList()
{
    qInfo() << "INFO: [GitPushDialog::updateCommitsList] Updating commits list";

    m_commitsWidget->clear();

    if (m_unpushedCommits.isEmpty()) {
        m_commitsCountLabel->setText(tr("No commits to push"));

        auto *item = new QListWidgetItem(tr("No unpushed commits found"));
        item->setIcon(QIcon(":/icons/vcs-normal"));
        m_commitsWidget->addItem(item);
    } else {
        m_commitsCountLabel->setText(tr("%1 commits to push").arg(m_unpushedCommits.size()));

        for (const auto &commit : m_unpushedCommits) {
            auto *item = new QListWidgetItem(formatCommitInfo(commit));
            item->setIcon(QIcon(":/icons/vcs-commit"));
            item->setToolTip(tr("Hash: %1\nAuthor: %2\nMessage: %3")
                                     .arg(commit.hash, commit.author, commit.message));
            m_commitsWidget->addItem(item);
        }
    }
}

QString GitPushDialog::getStatusDescription() const
{
    if (m_unpushedCommits.isEmpty()) {
        return tr("Repository is up to date with remote");
    }

    QString description = tr("Ready to push %1 commits to %2/%3")
                                  .arg(m_unpushedCommits.size())
                                  .arg(m_remoteCombo->currentText())
                                  .arg(m_remoteBranchCombo->currentText());

    if (m_forceCheckBox->isChecked()) {
        description += tr(" (Force Push)");
    }

    if (m_tagsCheckBox->isChecked()) {
        description += tr(" (Including Tags)");
    }

    return description;
}
