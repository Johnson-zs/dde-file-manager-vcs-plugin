#include "gitremotemanager.h"
#include "../gitoperationservice.h"
#include "../gitcommandexecutor.h"
#include "../utils.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QProgressBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QSplitter>
#include <QRegularExpression>
#include <QUrl>

GitRemoteManager::GitRemoteManager(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent)
    , m_repositoryPath(repositoryPath)
    , m_operationService(new GitOperationService(this))
    , m_isOperationInProgress(false)
{
    setWindowTitle(tr("Git Remote Manager"));
    setWindowIcon(QIcon(":/icons/vcs-branch"));
    setMinimumSize(800, 600);
    resize(900, 700);

    qInfo() << "INFO: [GitRemoteManager] Initializing remote manager for repository:" << repositoryPath;

    setupUI();
    setupConnections();
    loadRemotes();
}

GitRemoteManager::~GitRemoteManager()
{
    qInfo() << "INFO: [GitRemoteManager] Destroying remote manager";
}

void GitRemoteManager::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // 创建主分割器
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧面板 - 远程列表
    auto *leftWidget = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setSpacing(12);
    
    setupRemoteListGroup();
    leftLayout->addWidget(m_remoteListGroup);
    
    // 右侧面板 - 远程详情
    auto *rightWidget = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setSpacing(12);
    
    setupRemoteDetailsGroup();
    rightLayout->addWidget(m_detailsGroup);
    
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
    buttonLayout->setSpacing(8);

    buttonLayout->addStretch();

    // 关闭按钮
    m_closeButton = new QPushButton(tr("Close"));
    m_closeButton->setIcon(QIcon(":/icons/dialog-close"));
    m_closeButton->setDefault(true);

    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addWidget(buttonWidget);
}

void GitRemoteManager::setupRemoteListGroup()
{
    m_remoteListGroup = new QGroupBox(tr("Remote Repositories"));
    auto *layout = new QVBoxLayout(m_remoteListGroup);
    layout->setSpacing(8);

    // 远程数量标签
    m_remotesCountLabel = new QLabel(tr("Loading remotes..."));
    m_remotesCountLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_remotesCountLabel);

    // 远程列表
    m_remotesWidget = new QListWidget;
    m_remotesWidget->setAlternatingRowColors(true);
    m_remotesWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_remotesWidget);

    // 操作按钮
    auto *buttonLayout = new QHBoxLayout;
    
    m_addButton = new QPushButton(tr("Add"));
    m_addButton->setIcon(QIcon(":/icons/list-add"));
    m_addButton->setToolTip(tr("Add new remote repository"));
    
    m_removeButton = new QPushButton(tr("Remove"));
    m_removeButton->setIcon(QIcon(":/icons/list-remove"));
    m_removeButton->setToolTip(tr("Remove selected remote repository"));
    m_removeButton->setEnabled(false);
    
    m_refreshButton = new QPushButton(tr("Refresh"));
    m_refreshButton->setIcon(QIcon(":/icons/view-refresh"));
    m_refreshButton->setToolTip(tr("Refresh remote repositories list"));
    
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_refreshButton);
    
    layout->addLayout(buttonLayout);
}

void GitRemoteManager::setupRemoteDetailsGroup()
{
    m_detailsGroup = new QGroupBox(tr("Remote Details"));
    auto *layout = new QGridLayout(m_detailsGroup);
    layout->setSpacing(8);

    // 远程名称
    layout->addWidget(new QLabel(tr("Name:")), 0, 0);
    m_nameEdit = new QLineEdit;
    m_nameEdit->setReadOnly(true);
    layout->addWidget(m_nameEdit, 0, 1);

    // 拉取URL
    layout->addWidget(new QLabel(tr("Fetch URL:")), 1, 0);
    m_fetchUrlEdit = new QLineEdit;
    layout->addWidget(m_fetchUrlEdit, 1, 1);

    // 推送URL
    layout->addWidget(new QLabel(tr("Push URL:")), 2, 0);
    m_pushUrlEdit = new QLineEdit;
    layout->addWidget(m_pushUrlEdit, 2, 1);

    // 连接状态
    layout->addWidget(new QLabel(tr("Connection:")), 3, 0);
    m_connectionStatusLabel = new QLabel(tr("Unknown"));
    layout->addWidget(m_connectionStatusLabel, 3, 1);

    // 操作按钮
    auto *actionLayout = new QHBoxLayout;
    
    m_editButton = new QPushButton(tr("Save Changes"));
    m_editButton->setIcon(QIcon(":/icons/document-save"));
    m_editButton->setToolTip(tr("Save changes to remote configuration"));
    m_editButton->setEnabled(false);
    
    m_testButton = new QPushButton(tr("Test Connection"));
    m_testButton->setIcon(QIcon(":/icons/network-connect"));
    m_testButton->setToolTip(tr("Test connection to remote repository"));
    m_testButton->setEnabled(false);
    
    m_testAllButton = new QPushButton(tr("Test All"));
    m_testAllButton->setIcon(QIcon(":/icons/network-workgroup"));
    m_testAllButton->setToolTip(tr("Test connections to all remote repositories"));
    
    actionLayout->addWidget(m_editButton);
    actionLayout->addWidget(m_testButton);
    actionLayout->addStretch();
    actionLayout->addWidget(m_testAllButton);
    
    layout->addLayout(actionLayout, 4, 0, 1, 2);

    // 远程分支
    layout->addWidget(new QLabel(tr("Remote Branches:")), 5, 0, 1, 2);
    m_branchesCountLabel = new QLabel;
    layout->addWidget(m_branchesCountLabel, 6, 0, 1, 2);
    
    m_branchesWidget = new QListWidget;
    m_branchesWidget->setMaximumHeight(150);
    m_branchesWidget->setAlternatingRowColors(true);
    layout->addWidget(m_branchesWidget, 7, 0, 1, 2);

    layout->setColumnStretch(1, 1);
}

void GitRemoteManager::setupButtonGroup()
{
    // 按钮创建已移动到setupUI方法中，这里只保留空实现
}

void GitRemoteManager::setupConnections()
{
    // 列表选择信号
    connect(m_remotesWidget, &QListWidget::itemSelectionChanged,
            this, &GitRemoteManager::onRemoteSelectionChanged);

    // 按钮信号
    connect(m_addButton, &QPushButton::clicked, this, &GitRemoteManager::addRemote);
    connect(m_removeButton, &QPushButton::clicked, this, &GitRemoteManager::removeRemote);
    connect(m_editButton, &QPushButton::clicked, this, &GitRemoteManager::editRemote);
    connect(m_testButton, &QPushButton::clicked, this, &GitRemoteManager::testConnection);
    connect(m_testAllButton, &QPushButton::clicked, this, &GitRemoteManager::testAllConnections);
    connect(m_refreshButton, &QPushButton::clicked, this, &GitRemoteManager::refreshRemotes);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    // 编辑框变化信号
    connect(m_fetchUrlEdit, &QLineEdit::textChanged, [this]() {
        m_editButton->setEnabled(!m_selectedRemote.isEmpty() && !m_isOperationInProgress);
    });
    connect(m_pushUrlEdit, &QLineEdit::textChanged, [this]() {
        m_editButton->setEnabled(!m_selectedRemote.isEmpty() && !m_isOperationInProgress);
    });

    // 操作服务信号 - 修复参数匹配问题
    connect(m_operationService, &GitOperationService::operationCompleted,
            this, [this](const QString &operation, bool success, const QString &message) {
                Q_UNUSED(operation)
                onOperationCompleted(success, message);
            });
}

void GitRemoteManager::loadRemotes()
{
    qInfo() << "INFO: [GitRemoteManager::loadRemotes] Loading remote repositories";

    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "remote";
    cmd.arguments = QStringList() << "-v";
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);
    
    m_remotes.clear();
    m_remotesWidget->clear();

    if (result == GitCommandExecutor::Result::Success) {
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        QMap<QString, RemoteInfo> remoteMap;
        
        for (const QString &line : lines) {
            QStringList parts = line.split('\t');
            if (parts.size() >= 2) {
                QString remoteName = parts[0];
                QString urlAndType = parts[1];
                
                QStringList urlParts = urlAndType.split(' ');
                if (urlParts.size() >= 2) {
                    QString url = urlParts[0];
                    QString type = urlParts[1];
                    
                    if (!remoteMap.contains(remoteName)) {
                        RemoteInfo info;
                        info.name = remoteName;
                        info.isConnected = false;
                        remoteMap[remoteName] = info;
                    }
                    
                    if (type.contains("fetch")) {
                        remoteMap[remoteName].fetchUrl = url;
                    } else if (type.contains("push")) {
                        remoteMap[remoteName].pushUrl = url;
                    }
                }
            }
        }
        
        // 转换为向量并添加到列表
        for (auto it = remoteMap.begin(); it != remoteMap.end(); ++it) {
            RemoteInfo info = it.value();
            if (info.pushUrl.isEmpty()) {
                info.pushUrl = info.fetchUrl; // 默认推送URL与拉取URL相同
            }
            m_remotes.append(info);
            
            auto *item = new QListWidgetItem(formatRemoteInfo(info));
            item->setIcon(QIcon(":/icons/vcs-branch"));
            item->setData(Qt::UserRole, info.name);
            m_remotesWidget->addItem(item);
        }
        
        m_remotesCountLabel->setText(tr("%1 remote repositories").arg(m_remotes.size()));
        
        qInfo() << "INFO: [GitRemoteManager::loadRemotes] Loaded" << m_remotes.size() << "remotes";
    } else {
        qWarning() << "WARNING: [GitRemoteManager::loadRemotes] Failed to load remotes:" << error;
        m_remotesCountLabel->setText(tr("Failed to load remotes"));
        
        auto *item = new QListWidgetItem(tr("No remote repositories found"));
        item->setIcon(QIcon(":/icons/dialog-warning"));
        m_remotesWidget->addItem(item);
    }
}

void GitRemoteManager::loadRemoteDetails(const QString &remoteName)
{
    qInfo() << "INFO: [GitRemoteManager::loadRemoteDetails] Loading details for remote:" << remoteName;

    // 查找远程信息
    RemoteInfo *info = nullptr;
    for (auto &remote : m_remotes) {
        if (remote.name == remoteName) {
            info = &remote;
            break;
        }
    }
    
    if (!info) {
        qWarning() << "WARNING: [GitRemoteManager::loadRemoteDetails] Remote not found:" << remoteName;
        return;
    }
    
    // 更新UI
    m_nameEdit->setText(info->name);
    m_fetchUrlEdit->setText(info->fetchUrl);
    m_pushUrlEdit->setText(info->pushUrl);
    
    if (info->isConnected) {
        m_connectionStatusLabel->setText(tr("Connected"));
        m_connectionStatusLabel->setStyleSheet("color: #4CAF50;");
    } else {
        m_connectionStatusLabel->setText(tr("Unknown"));
        m_connectionStatusLabel->setStyleSheet("color: #FF9800;");
    }
    
    // 加载远程分支
    m_branchesWidget->clear();
    QStringList branches = m_operationService->getRemoteBranches(m_repositoryPath, remoteName);
    info->branches = branches;
    
    m_branchesCountLabel->setText(tr("%1 branches").arg(branches.size()));
    
    for (const QString &branch : branches) {
        auto *item = new QListWidgetItem(branch);
        item->setIcon(QIcon(":/icons/vcs-branch"));
        m_branchesWidget->addItem(item);
    }
}

// 槽函数实现
void GitRemoteManager::onRemoteSelectionChanged()
{
    auto *currentItem = m_remotesWidget->currentItem();
    bool hasSelection = currentItem != nullptr;
    
    m_removeButton->setEnabled(hasSelection && !m_isOperationInProgress);
    m_editButton->setEnabled(false); // 只有在编辑时才启用
    m_testButton->setEnabled(hasSelection && !m_isOperationInProgress);
    
    if (hasSelection) {
        m_selectedRemote = currentItem->data(Qt::UserRole).toString();
        loadRemoteDetails(m_selectedRemote);
    } else {
        m_selectedRemote.clear();
        m_nameEdit->clear();
        m_fetchUrlEdit->clear();
        m_pushUrlEdit->clear();
        m_connectionStatusLabel->setText(tr("No remote selected"));
        m_connectionStatusLabel->setStyleSheet("");
        m_branchesCountLabel->clear();
        m_branchesWidget->clear();
    }
}

void GitRemoteManager::addRemote()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add Remote Repository"),
                                        tr("Remote name:"), QLineEdit::Normal,
                                        "", &ok);
    if (!ok || name.isEmpty()) {
        return;
    }
    
    if (!validateRemoteName(name)) {
        QMessageBox::warning(this, tr("Invalid Name"),
                           tr("Remote name '%1' is invalid or already exists.").arg(name));
        return;
    }
    
    QString url = QInputDialog::getText(this, tr("Add Remote Repository"),
                                       tr("Remote URL:"), QLineEdit::Normal,
                                       "", &ok);
    if (!ok || url.isEmpty()) {
        return;
    }
    
    if (!validateRemoteUrl(url)) {
        QMessageBox::warning(this, tr("Invalid URL"),
                           tr("Remote URL '%1' is invalid.").arg(url));
        return;
    }
    
    addNewRemote(name, url);
}

void GitRemoteManager::removeRemote()
{
    if (m_selectedRemote.isEmpty()) {
        return;
    }
    
    int ret = QMessageBox::question(this, tr("Remove Remote"),
                                   tr("Are you sure you want to remove remote '%1'?\n"
                                      "This action cannot be undone.").arg(m_selectedRemote),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        deleteRemote(m_selectedRemote);
    }
}

void GitRemoteManager::editRemote()
{
    if (m_selectedRemote.isEmpty()) {
        return;
    }
    
    QString fetchUrl = m_fetchUrlEdit->text().trimmed();
    QString pushUrl = m_pushUrlEdit->text().trimmed();
    
    if (fetchUrl.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid URL"), tr("Fetch URL cannot be empty."));
        return;
    }
    
    if (!validateRemoteUrl(fetchUrl)) {
        QMessageBox::warning(this, tr("Invalid URL"), tr("Fetch URL is invalid."));
        return;
    }
    
    if (!pushUrl.isEmpty() && !validateRemoteUrl(pushUrl)) {
        QMessageBox::warning(this, tr("Invalid URL"), tr("Push URL is invalid."));
        return;
    }
    
    updateRemoteUrl(m_selectedRemote, fetchUrl);
}

void GitRemoteManager::testConnection()
{
    if (m_selectedRemote.isEmpty()) {
        return;
    }
    
    showProgress(tr("Testing connection to %1...").arg(m_selectedRemote));
    
    bool success = m_operationService->testRemoteConnection(m_repositoryPath, m_selectedRemote);
    
    hideProgress();
    
    // 更新连接状态
    for (auto &remote : m_remotes) {
        if (remote.name == m_selectedRemote) {
            remote.isConnected = success;
            break;
        }
    }
    
    if (success) {
        m_connectionStatusLabel->setText(tr("Connected"));
        m_connectionStatusLabel->setStyleSheet("color: #4CAF50;");
        QMessageBox::information(this, tr("Connection Test"),
                               tr("Successfully connected to remote '%1'.").arg(m_selectedRemote));
    } else {
        m_connectionStatusLabel->setText(tr("Connection Failed"));
        m_connectionStatusLabel->setStyleSheet("color: #F44336;");
        QMessageBox::warning(this, tr("Connection Test"),
                           tr("Failed to connect to remote '%1'.").arg(m_selectedRemote));
    }
}

void GitRemoteManager::testAllConnections()
{
    if (m_remotes.isEmpty()) {
        return;
    }
    
    showProgress(tr("Testing all connections..."));
    
    int successCount = 0;
    for (auto &remote : m_remotes) {
        bool success = m_operationService->testRemoteConnection(m_repositoryPath, remote.name);
        remote.isConnected = success;
        if (success) {
            successCount++;
        }
    }
    
    hideProgress();
    
    // 刷新当前选择的远程详情
    if (!m_selectedRemote.isEmpty()) {
        loadRemoteDetails(m_selectedRemote);
    }
    
    QMessageBox::information(this, tr("Connection Test Results"),
                           tr("Successfully connected to %1 out of %2 remotes.")
                           .arg(successCount).arg(m_remotes.size()));
}

void GitRemoteManager::refreshRemotes()
{
    qInfo() << "INFO: [GitRemoteManager::refreshRemotes] Refreshing remote repositories";
    loadRemotes();
}

void GitRemoteManager::onOperationCompleted(bool success, const QString &message)
{
    hideProgress();
    
    if (success) {
        qInfo() << "INFO: [GitRemoteManager::onOperationCompleted] Operation completed successfully";
        refreshRemotes();
    } else {
        qWarning() << "WARNING: [GitRemoteManager::onOperationCompleted] Operation failed:" << message;
        QMessageBox::critical(this, tr("Operation Failed"), message);
    }
}

// 操作方法实现
void GitRemoteManager::addNewRemote(const QString &name, const QString &url)
{
    qInfo() << "INFO: [GitRemoteManager::addNewRemote] Adding remote:" << name << "url:" << url;
    
    showProgress(tr("Adding remote '%1'...").arg(name));
    m_operationService->addRemote(m_repositoryPath, name, url);
}

void GitRemoteManager::updateRemoteUrl(const QString &name, const QString &url)
{
    qInfo() << "INFO: [GitRemoteManager::updateRemoteUrl] Updating remote URL:" << name << "url:" << url;
    
    showProgress(tr("Updating remote '%1'...").arg(name));
    m_operationService->setRemoteUrl(m_repositoryPath, name, url);
}

void GitRemoteManager::deleteRemote(const QString &name)
{
    qInfo() << "INFO: [GitRemoteManager::deleteRemote] Deleting remote:" << name;
    
    showProgress(tr("Removing remote '%1'...").arg(name));
    m_operationService->removeRemote(m_repositoryPath, name);
}

bool GitRemoteManager::validateRemoteName(const QString &name) const
{
    if (name.isEmpty() || name.contains(' ') || name.contains('\t')) {
        return false;
    }
    
    // 检查是否已存在
    for (const auto &remote : m_remotes) {
        if (remote.name == name) {
            return false;
        }
    }
    
    return true;
}

bool GitRemoteManager::validateRemoteUrl(const QString &url) const
{
    if (url.isEmpty()) {
        return false;
    }
    
    // 简单的URL验证
    QUrl qurl(url);
    if (qurl.isValid() && (qurl.scheme() == "http" || qurl.scheme() == "https" || 
                          qurl.scheme() == "git" || qurl.scheme() == "ssh")) {
        return true;
    }
    
    // SSH格式验证 (user@host:path)
    QRegularExpression sshRegex(R"(^[^@]+@[^:]+:.+$)");
    if (sshRegex.match(url).hasMatch()) {
        return true;
    }
    
    return false;
}

// 辅助方法实现
void GitRemoteManager::enableControls(bool enabled)
{
    m_addButton->setEnabled(enabled);
    m_removeButton->setEnabled(enabled && !m_selectedRemote.isEmpty());
    m_editButton->setEnabled(enabled && !m_selectedRemote.isEmpty());
    m_testButton->setEnabled(enabled && !m_selectedRemote.isEmpty());
    m_testAllButton->setEnabled(enabled);
    m_refreshButton->setEnabled(enabled);
    m_remotesWidget->setEnabled(enabled);
    m_fetchUrlEdit->setEnabled(enabled);
    m_pushUrlEdit->setEnabled(enabled);
}

void GitRemoteManager::showProgress(const QString &message)
{
    m_isOperationInProgress = true;
    enableControls(false);
    
    m_progressBar->setVisible(true);
    m_progressLabel->setVisible(true);
    m_progressBar->setRange(0, 0); // 不确定进度
    m_progressLabel->setText(message);
}

void GitRemoteManager::hideProgress()
{
    m_isOperationInProgress = false;
    enableControls(true);
    
    m_progressBar->setVisible(false);
    m_progressLabel->setVisible(false);
}

QString GitRemoteManager::formatRemoteInfo(const RemoteInfo &info) const
{
    return QString("%1 (%2)").arg(info.name, info.fetchUrl);
} 