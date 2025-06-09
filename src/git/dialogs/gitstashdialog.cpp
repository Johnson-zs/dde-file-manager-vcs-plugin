#include "gitstashdialog.h"
#include "../gitoperationservice.h"
#include "../gitcommandexecutor.h"
#include "../widgets/linenumbertextedit.h"
#include "../gitstashutils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QApplication>
#include <QHeaderView>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>

GitStashDialog::GitStashDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent)
    , m_repositoryPath(repositoryPath)
    , m_operationService(new GitOperationService(this))
    , m_mainSplitter(nullptr)
    , m_stashListGroup(nullptr)
    , m_stashListWidget(nullptr)
    , m_stashCountLabel(nullptr)
    , m_previewGroup(nullptr)
    , m_previewTitleLabel(nullptr)
    , m_previewTextEdit(nullptr)
    , m_buttonGroup(nullptr)
    , m_contextMenu(nullptr)
{
    setWindowTitle(tr("Git Stash Manager - %1").arg(repositoryPath));
    setWindowIcon(QIcon(":/icons/vcs-stash"));
    resize(900, 600);
    
    setupUI();
    setupContextMenu();
    loadStashList();
    
    // 连接操作服务信号
    connect(m_operationService, &GitOperationService::operationCompleted,
            this, &GitStashDialog::onOperationCompleted);
    
    qInfo() << "INFO: [GitStashDialog] Initialized stash dialog for repository:" << repositoryPath;
}

void GitStashDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    setupStashList();
    setupPreviewArea();
    
    // 设置分割器比例
    m_mainSplitter->addWidget(m_stashListGroup);
    m_mainSplitter->addWidget(m_previewGroup);
    m_mainSplitter->setStretchFactor(0, 1);  // 左侧stash列表
    m_mainSplitter->setStretchFactor(1, 2);  // 右侧预览区域
    
    mainLayout->addWidget(m_mainSplitter);
    
    setupButtonArea();
    mainLayout->addWidget(m_buttonGroup);
}

void GitStashDialog::setupStashList()
{
    m_stashListGroup = new QGroupBox(tr("Stash List"), this);
    auto *listLayout = new QVBoxLayout(m_stashListGroup);
    
    // Stash计数标签
    m_stashCountLabel = new QLabel(tr("No stashes found"), this);
    m_stashCountLabel->setStyleSheet("QLabel { color: #666; font-size: 12px; }");
    listLayout->addWidget(m_stashCountLabel);
    
    // Stash列表
    m_stashListWidget = new QListWidget(this);
    m_stashListWidget->setAlternatingRowColors(true);
    m_stashListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stashListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 设置列表样式
    m_stashListWidget->setStyleSheet(
        "QListWidget {"
        "    border: 1px solid #ccc;"
        "    border-radius: 4px;"
        "    background-color: white;"
        "    selection-background-color: #3daee9;"
        "}"
        "QListWidget::item {"
        "    padding: 8px;"
        "    border-bottom: 1px solid #eee;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #3daee9;"
        "    color: white;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #f0f0f0;"
        "}"
    );
    
    listLayout->addWidget(m_stashListWidget);
    
    // 连接信号
    connect(m_stashListWidget, &QListWidget::itemSelectionChanged,
            this, &GitStashDialog::onStashSelectionChanged);
    connect(m_stashListWidget, &QListWidget::itemDoubleClicked,
            this, &GitStashDialog::onStashDoubleClicked);
    connect(m_stashListWidget, &QListWidget::customContextMenuRequested,
            this, &GitStashDialog::showStashContextMenu);
}

void GitStashDialog::setupPreviewArea()
{
    m_previewGroup = new QGroupBox(tr("Stash Content Preview"), this);
    auto *previewLayout = new QVBoxLayout(m_previewGroup);
    
    // 预览标题
    m_previewTitleLabel = new QLabel(tr("Select a stash to preview its content"), this);
    m_previewTitleLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; }");
    previewLayout->addWidget(m_previewTitleLabel);
    
    // 预览文本区域（带行号）
    m_previewTextEdit = new LineNumberTextEdit(this);
    m_previewTextEdit->setReadOnly(true);
    m_previewTextEdit->setFont(QFont("Consolas", 10));
    m_previewTextEdit->setStyleSheet(
        "QTextEdit {"
        "    border: 1px solid #ccc;"
        "    border-radius: 4px;"
        "    background-color: #fafafa;"
        "}"
    );
    
    previewLayout->addWidget(m_previewTextEdit);
}

void GitStashDialog::setupButtonArea()
{
    m_buttonGroup = new QGroupBox(this);
    m_buttonGroup->setFlat(true);
    auto *buttonLayout = new QHBoxLayout(m_buttonGroup);
    buttonLayout->setContentsMargins(0, 8, 0, 0);
    
    // 左侧操作按钮
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setIcon(QIcon(":/icons/view-refresh"));
    m_refreshButton->setToolTip(tr("Refresh stash list"));
    
    m_createStashButton = new QPushButton(tr("New Stash"), this);
    m_createStashButton->setIcon(QIcon(":/icons/vcs-stash"));
    m_createStashButton->setToolTip(tr("Create a new stash"));
    
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addWidget(m_createStashButton);
    buttonLayout->addSpacing(20);
    
    // 中间stash操作按钮
    m_applyButton = new QPushButton(tr("Apply"), this);
    m_applyButton->setIcon(QIcon(":/icons/vcs-update-required"));
    m_applyButton->setToolTip(tr("Apply selected stash and remove it"));
    
    m_applyKeepButton = new QPushButton(tr("Apply & Keep"), this);
    m_applyKeepButton->setIcon(QIcon(":/icons/vcs-added"));
    m_applyKeepButton->setToolTip(tr("Apply selected stash but keep it in the list"));
    
    m_deleteButton = new QPushButton(tr("Delete"), this);
    m_deleteButton->setIcon(QIcon(":/icons/edit-delete"));
    m_deleteButton->setToolTip(tr("Delete selected stash"));
    
    m_createBranchButton = new QPushButton(tr("Create Branch"), this);
    m_createBranchButton->setIcon(QIcon(":/icons/vcs-branch"));
    m_createBranchButton->setToolTip(tr("Create a new branch from selected stash"));
    
    m_showDiffButton = new QPushButton(tr("Show Diff"), this);
    m_showDiffButton->setIcon(QIcon(":/icons/vcs-diff"));
    m_showDiffButton->setToolTip(tr("Show detailed diff of selected stash"));
    
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_applyKeepButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(m_createBranchButton);
    buttonLayout->addWidget(m_showDiffButton);
    
    buttonLayout->addStretch();
    
    // 右侧关闭按钮
    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setIcon(QIcon(":/icons/dialog-close"));
    buttonLayout->addWidget(m_closeButton);
    
    // 连接信号
    connect(m_refreshButton, &QPushButton::clicked, this, &GitStashDialog::onRefreshClicked);
    connect(m_createStashButton, &QPushButton::clicked, this, &GitStashDialog::onCreateStashClicked);
    connect(m_applyButton, &QPushButton::clicked, this, &GitStashDialog::onApplyStashClicked);
    connect(m_applyKeepButton, &QPushButton::clicked, this, &GitStashDialog::onApplyKeepStashClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &GitStashDialog::onDeleteStashClicked);
    connect(m_createBranchButton, &QPushButton::clicked, this, &GitStashDialog::onCreateBranchClicked);
    connect(m_showDiffButton, &QPushButton::clicked, this, &GitStashDialog::onShowDiffClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    
    // 初始状态下禁用需要选择stash的按钮
    updateButtonStates();
}

void GitStashDialog::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    m_applyAction = m_contextMenu->addAction(QIcon(":/icons/vcs-update-required"), tr("Apply"));
    m_applyKeepAction = m_contextMenu->addAction(QIcon(":/icons/vcs-added"), tr("Apply && Keep"));
    m_contextMenu->addSeparator();
    m_deleteAction = m_contextMenu->addAction(QIcon(":/icons/edit-delete"), tr("Delete"));
    m_createBranchAction = m_contextMenu->addAction(QIcon(":/icons/vcs-branch"), tr("Create Branch"));
    m_contextMenu->addSeparator();
    m_showDiffAction = m_contextMenu->addAction(QIcon(":/icons/vcs-diff"), tr("Show Diff"));
    m_contextMenu->addSeparator();
    m_refreshAction = m_contextMenu->addAction(QIcon(":/icons/view-refresh"), tr("Refresh"));
    
    // 连接右键菜单信号
    connect(m_applyAction, &QAction::triggered, this, &GitStashDialog::onApplyStashClicked);
    connect(m_applyKeepAction, &QAction::triggered, this, &GitStashDialog::onApplyKeepStashClicked);
    connect(m_deleteAction, &QAction::triggered, this, &GitStashDialog::onDeleteStashClicked);
    connect(m_createBranchAction, &QAction::triggered, this, &GitStashDialog::onCreateBranchClicked);
    connect(m_showDiffAction, &QAction::triggered, this, &GitStashDialog::onShowDiffClicked);
    connect(m_refreshAction, &QAction::triggered, this, &GitStashDialog::onRefreshClicked);
}

void GitStashDialog::loadStashList()
{
    qInfo() << "INFO: [GitStashDialog::loadStashList] Loading stash list for repository:" << m_repositoryPath;
    
    m_stashListWidget->clear();
    m_stashList.clear();
    
    QStringList stashLines = m_operationService->listStashes(m_repositoryPath);
    
    if (stashLines.isEmpty()) {
        m_stashCountLabel->setText(tr("No stashes found"));
        clearPreview();
        updateButtonStates();
        return;
    }
    
    // 解析stash列表
    for (const QString &line : stashLines) {
        GitStashInfo info = GitStashUtils::parseStashLine(line);
        if (info.isValid()) {
            m_stashList.append(info);
            
            auto *item = new QListWidgetItem(GitStashUtils::formatStashDisplayText(info));
            item->setData(Qt::UserRole, info.index);
            item->setToolTip(tr("Stash: %1\nBranch: %2\nAuthor: %3\nTime: %4")
                           .arg(info.message, info.branch, info.author, 
                                info.timestamp.toString("yyyy-MM-dd hh:mm:ss")));
            
            m_stashListWidget->addItem(item);
        }
    }
    
    m_stashCountLabel->setText(tr("Found %1 stash(es)").arg(m_stashList.size()));
    updateButtonStates();
    
    qInfo() << "INFO: [GitStashDialog::loadStashList] Loaded" << m_stashList.size() << "stashes";
}

void GitStashDialog::onRefreshClicked()
{
    qInfo() << "INFO: [GitStashDialog::onRefreshClicked] Refreshing stash list";
    loadStashList();
}

void GitStashDialog::onStashSelectionChanged()
{
    updateButtonStates();
    
    if (hasSelectedStash()) {
        int stashIndex = getSelectedStashIndex();
        refreshStashPreview(stashIndex);
    } else {
        clearPreview();
    }
}

void GitStashDialog::onStashDoubleClicked(QListWidgetItem *item)
{
    Q_UNUSED(item)
    // 双击应用stash
    onApplyStashClicked();
}

void GitStashDialog::onCreateStashClicked()
{
    createNewStash();
}

void GitStashDialog::onApplyStashClicked()
{
    applySelectedStash(false);
}

void GitStashDialog::onApplyKeepStashClicked()
{
    applySelectedStash(true);
}

void GitStashDialog::onDeleteStashClicked()
{
    deleteSelectedStash();
}

void GitStashDialog::onCreateBranchClicked()
{
    createBranchFromSelectedStash();
}

void GitStashDialog::onShowDiffClicked()
{
    showSelectedStashDiff();
}

void GitStashDialog::showStashContextMenu(const QPoint &pos)
{
    if (m_stashListWidget->itemAt(pos)) {
        bool hasSelection = hasSelectedStash();
        
        m_applyAction->setEnabled(hasSelection);
        m_applyKeepAction->setEnabled(hasSelection);
        m_deleteAction->setEnabled(hasSelection);
        m_createBranchAction->setEnabled(hasSelection);
        m_showDiffAction->setEnabled(hasSelection);
        
        m_contextMenu->exec(m_stashListWidget->mapToGlobal(pos));
    }
}

void GitStashDialog::onOperationCompleted(const QString &operation, bool success, const QString &message)
{
    qInfo() << "INFO: [GitStashDialog::onOperationCompleted] Operation:" << operation 
            << "Success:" << success << "Message:" << message;
    
    if (success) {
        // 操作成功，刷新stash列表
        loadStashList();
        
        if (operation.contains("Apply") || operation.contains("Delete")) {
            // 如果是应用或删除操作，清空预览
            clearPreview();
        }
    } else {
        // 操作失败，显示错误消息
        QMessageBox::warning(this, tr("Operation Failed"), 
                           tr("Failed to %1:\n%2").arg(operation.toLower(), message));
    }
}

void GitStashDialog::refreshStashPreview(int stashIndex)
{
    qInfo() << "INFO: [GitStashDialog::refreshStashPreview] Previewing stash" << stashIndex;
    
    m_previewTitleLabel->setText(tr("Stash@{%1} Content Preview").arg(stashIndex));
    
    // 执行git stash show命令获取stash内容
    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "stash";
    cmd.arguments = QStringList() << "stash" << "show" << "-p" << QString("stash@{%1}").arg(stashIndex);
    cmd.workingDirectory = m_repositoryPath;
    cmd.timeout = 10000;
    
    auto result = executor.executeCommand(cmd, output, error);
    
    if (result == GitCommandExecutor::Result::Success) {
        m_previewTextEdit->setPlainText(output);
        qInfo() << "INFO: [GitStashDialog::refreshStashPreview] Successfully loaded stash preview";
    } else {
        m_previewTextEdit->setPlainText(tr("Failed to load stash content:\n%1").arg(error));
        qWarning() << "WARNING: [GitStashDialog::refreshStashPreview] Failed to load stash content:" << error;
    }
}

void GitStashDialog::updateButtonStates()
{
    bool hasSelection = hasSelectedStash();
    
    m_applyButton->setEnabled(hasSelection);
    m_applyKeepButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
    m_createBranchButton->setEnabled(hasSelection);
    m_showDiffButton->setEnabled(hasSelection);
}

void GitStashDialog::clearPreview()
{
    m_previewTitleLabel->setText(tr("Select a stash to preview its content"));
    m_previewTextEdit->clear();
}

void GitStashDialog::createNewStash()
{
    bool ok;
    QString message = QInputDialog::getText(this, tr("Create New Stash"),
                                          tr("Enter stash message:"), QLineEdit::Normal,
                                          tr("Work in progress"), &ok);
    
    if (ok && !message.isEmpty()) {
        qInfo() << "INFO: [GitStashDialog::createNewStash] Creating stash with message:" << message;
        m_operationService->createStash(m_repositoryPath.toStdString(), message);
    }
}

void GitStashDialog::applySelectedStash(bool keepStash)
{
    if (!hasSelectedStash()) {
        return;
    }
    
    int stashIndex = getSelectedStashIndex();
    GitStashInfo info = getSelectedStashInfo();
    
    QString operation = keepStash ? tr("apply and keep") : tr("apply");
    int ret = QMessageBox::question(this, tr("Apply Stash"),
                                   tr("Are you sure you want to %1 stash@{%2}?\n\n"
                                      "Message: %3")
                                           .arg(operation)
                                           .arg(stashIndex)
                                           .arg(info.message),
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        qInfo() << "INFO: [GitStashDialog::applySelectedStash] Applying stash" << stashIndex << "keep:" << keepStash;
        m_operationService->applyStash(m_repositoryPath.toStdString(), stashIndex, keepStash);
    }
}

void GitStashDialog::deleteSelectedStash()
{
    if (!hasSelectedStash()) {
        return;
    }
    
    int stashIndex = getSelectedStashIndex();
    GitStashInfo info = getSelectedStashInfo();
    
    int ret = QMessageBox::question(this, tr("Delete Stash"),
                                   tr("Are you sure you want to delete stash@{%1}?\n\n"
                                      "Message: %2\n\n"
                                      "This action cannot be undone!")
                                           .arg(stashIndex)
                                           .arg(info.message),
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        qInfo() << "INFO: [GitStashDialog::deleteSelectedStash] Deleting stash" << stashIndex;
        m_operationService->deleteStash(m_repositoryPath.toStdString(), stashIndex);
    }
}

void GitStashDialog::createBranchFromSelectedStash()
{
    if (!hasSelectedStash()) {
        return;
    }
    
    int stashIndex = getSelectedStashIndex();
    GitStashInfo info = getSelectedStashInfo();
    
    bool ok;
    QString branchName = QInputDialog::getText(this, tr("Create Branch from Stash"),
                                             tr("Enter new branch name:"), QLineEdit::Normal,
                                             QString("stash-branch-%1").arg(stashIndex), &ok);
    
    if (ok && !branchName.isEmpty()) {
        qInfo() << "INFO: [GitStashDialog::createBranchFromSelectedStash] Creating branch" << branchName 
                << "from stash" << stashIndex;
        m_operationService->createBranchFromStash(m_repositoryPath.toStdString(), stashIndex, branchName);
    }
}

void GitStashDialog::showSelectedStashDiff()
{
    if (!hasSelectedStash()) {
        return;
    }
    
    int stashIndex = getSelectedStashIndex();
    qInfo() << "INFO: [GitStashDialog::showSelectedStashDiff] Showing diff for stash" << stashIndex;
    m_operationService->showStashDiff(m_repositoryPath.toStdString(), stashIndex);
}

int GitStashDialog::getSelectedStashIndex() const
{
    QListWidgetItem *item = m_stashListWidget->currentItem();
    if (item) {
        return item->data(Qt::UserRole).toInt();
    }
    return -1;
}

GitStashInfo GitStashDialog::getSelectedStashInfo() const
{
    int index = getSelectedStashIndex();
    if (index >= 0) {
        for (const GitStashInfo &info : m_stashList) {
            if (info.index == index) {
                return info;
            }
        }
    }
    return GitStashInfo();
}

bool GitStashDialog::hasSelectedStash() const
{
    return getSelectedStashIndex() >= 0;
}

void GitStashDialog::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_F5:
        onRefreshClicked();
        break;
    case Qt::Key_Delete:
        if (hasSelectedStash()) {
            deleteSelectedStash();
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (hasSelectedStash()) {
            applySelectedStash(false);
        }
        break;
    default:
        QDialog::keyPressEvent(event);
        break;
    }
}

bool GitStashDialog::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    Q_UNUSED(event)
    return false;
} 