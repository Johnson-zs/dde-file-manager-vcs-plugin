#include "gitcommitdialog.h"
#include "gitoperationdialog.h"
#include "../gitcommandexecutor.h"
#include "../utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QDebug>
#include <QSplitter>
#include <QApplication>
#include <QHeaderView>
#include <QFileInfo>
#include <QTimer>

GitCommitDialog::GitCommitDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent)
    , m_repositoryPath(repositoryPath)
    , m_isAmendMode(false)
    , m_isAllowEmpty(false)
    , m_isUpdating(false)
    , m_mainSplitter(nullptr)
    , m_optionsGroup(nullptr)
    , m_amendCheckBox(nullptr)
    , m_allowEmptyCheckBox(nullptr)
    , m_optionsLabel(nullptr)
    , m_messageGroup(nullptr)
    , m_messageEdit(nullptr)
    , m_messageHintLabel(nullptr)
    , m_filesGroup(nullptr)
    , m_fileFilterCombo(nullptr)
    , m_fileSearchEdit(nullptr)
    , m_changedFilesList(nullptr)
    , m_stagedCountLabel(nullptr)
    , m_modifiedCountLabel(nullptr)
    , m_untrackedCountLabel(nullptr)
    , m_refreshButton(nullptr)
    , m_stageSelectedButton(nullptr)
    , m_unstageSelectedButton(nullptr)
    , m_selectAllButton(nullptr)
    , m_selectNoneButton(nullptr)
    , m_commitButton(nullptr)
    , m_cancelButton(nullptr)
{
    setWindowTitle(tr("Git Commit"));
    setModal(true);
    setMinimumSize(800, 700);
    // Enable maximize button and better default size
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(1200, 800);
    setAttribute(Qt::WA_DeleteOnClose);
    
    setupUI();
    loadChangedFiles();
    
    qDebug() << "[GitCommitDialog] Initialized for repository:" << repositoryPath;
}

GitCommitDialog::GitCommitDialog(const QString &repositoryPath, const QStringList &files, QWidget *parent)
    : GitCommitDialog(repositoryPath, parent)
{
    m_files = files;
    refreshFilesList();
}

void GitCommitDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // === 选项区域 ===
    m_optionsGroup = new QGroupBox(tr("Commit Options"), this);
    auto *optionsLayout = new QVBoxLayout(m_optionsGroup);
    
    // 选项说明
    m_optionsLabel = new QLabel(tr("Select commit type and options:"), this);
    m_optionsLabel->setStyleSheet("color: #666; font-size: 11px;");
    optionsLayout->addWidget(m_optionsLabel);
    
    // 选项复选框布局
    auto *checkBoxLayout = new QHBoxLayout();
    
    m_amendCheckBox = new QCheckBox(tr("Amend last commit"), this);
    m_amendCheckBox->setToolTip(tr("Modify the most recent commit instead of creating a new one.\nThis will replace the last commit with the current staging area content."));
    checkBoxLayout->addWidget(m_amendCheckBox);
    
    m_allowEmptyCheckBox = new QCheckBox(tr("Allow empty commit"), this);
    m_allowEmptyCheckBox->setToolTip(tr("Allow creating a commit without any changes.\nUseful for triggering CI/CD pipelines or marking milestones."));
    checkBoxLayout->addWidget(m_allowEmptyCheckBox);
    
    checkBoxLayout->addStretch();
    optionsLayout->addLayout(checkBoxLayout);
    
    mainLayout->addWidget(m_optionsGroup);

    // === 主分割器 ===
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    
    // === 提交消息区域 ===
    m_messageGroup = new QGroupBox(tr("Commit Message"), this);
    auto *messageLayout = new QVBoxLayout(m_messageGroup);
    
    m_messageHintLabel = new QLabel(tr("Enter a clear and descriptive commit message:"), this);
    m_messageHintLabel->setStyleSheet("color: #666; font-size: 11px;");
    messageLayout->addWidget(m_messageHintLabel);
    
    m_messageEdit = new QTextEdit(this);
    m_messageEdit->setMaximumHeight(150);
    m_messageEdit->setPlaceholderText(tr("feat: add new feature\n\nDetailed description of the changes..."));
    m_messageEdit->setFont(QFont("Courier", 10));
    messageLayout->addWidget(m_messageEdit);
    
    m_mainSplitter->addWidget(m_messageGroup);

    // === 文件选择区域 ===
    m_filesGroup = new QGroupBox(tr("Changed Files"), this);
    auto *filesLayout = new QVBoxLayout(m_filesGroup);
    
    // 过滤和搜索工具栏
    auto *filterLayout = new QHBoxLayout();
    
    filterLayout->addWidget(new QLabel(tr("Filter:"), this));
    m_fileFilterCombo = new QComboBox(this);
    m_fileFilterCombo->addItem(tr("All files"), "all");
    m_fileFilterCombo->addItem(tr("Staged files"), "staged");
    m_fileFilterCombo->addItem(tr("Modified files"), "modified");
    m_fileFilterCombo->addItem(tr("Untracked files"), "untracked");
    filterLayout->addWidget(m_fileFilterCombo);
    
    filterLayout->addSpacing(10);
    filterLayout->addWidget(new QLabel(tr("Search:"), this));
    m_fileSearchEdit = new QLineEdit(this);
    m_fileSearchEdit->setPlaceholderText(tr("Search files..."));
    filterLayout->addWidget(m_fileSearchEdit);
    
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setIcon(QIcon::fromTheme("view-refresh"));
    filterLayout->addWidget(m_refreshButton);
    
    filterLayout->addStretch();
    filesLayout->addLayout(filterLayout);
    
    // 状态统计标签
    auto *statsLayout = new QHBoxLayout();
    m_stagedCountLabel = new QLabel(this);
    m_modifiedCountLabel = new QLabel(this);
    m_untrackedCountLabel = new QLabel(this);
    statsLayout->addWidget(m_stagedCountLabel);
    statsLayout->addWidget(m_modifiedCountLabel);
    statsLayout->addWidget(m_untrackedCountLabel);
    statsLayout->addStretch();
    filesLayout->addLayout(statsLayout);
    
    // 文件列表
    m_changedFilesList = new QTreeWidget(this);
    m_changedFilesList->setHeaderLabels({tr("File"), tr("Status"), tr("Path")});
    m_changedFilesList->setRootIsDecorated(false);
    m_changedFilesList->setAlternatingRowColors(true);
    m_changedFilesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_changedFilesList->header()->setStretchLastSection(true);
    m_changedFilesList->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_changedFilesList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    filesLayout->addWidget(m_changedFilesList);
    
    // 文件操作按钮
    auto *fileButtonLayout = new QHBoxLayout();
    m_stageSelectedButton = new QPushButton(tr("Stage Selected"), this);
    m_stageSelectedButton->setIcon(QIcon::fromTheme("list-add"));
    fileButtonLayout->addWidget(m_stageSelectedButton);
    
    m_unstageSelectedButton = new QPushButton(tr("Unstage Selected"), this);
    m_unstageSelectedButton->setIcon(QIcon::fromTheme("list-remove"));
    fileButtonLayout->addWidget(m_unstageSelectedButton);
    
    fileButtonLayout->addSpacing(10);
    
    m_selectAllButton = new QPushButton(tr("Select All"), this);
    fileButtonLayout->addWidget(m_selectAllButton);
    
    m_selectNoneButton = new QPushButton(tr("Select None"), this);
    fileButtonLayout->addWidget(m_selectNoneButton);
    
    fileButtonLayout->addStretch();
    filesLayout->addLayout(fileButtonLayout);
    
    m_mainSplitter->addWidget(m_filesGroup);
    
    // 设置分割器比例
    m_mainSplitter->setStretchFactor(0, 0);  // 消息区域固定高度
    m_mainSplitter->setStretchFactor(1, 1);  // 文件区域可拉伸
    
    mainLayout->addWidget(m_mainSplitter);

    // === 按钮区域 ===
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    buttonLayout->addWidget(m_cancelButton);
    
    m_commitButton = new QPushButton(tr("Commit"), this);
    m_commitButton->setDefault(true);
    m_commitButton->setEnabled(false);
    m_commitButton->setStyleSheet("QPushButton { font-weight: bold; padding: 8px 16px; }");
    buttonLayout->addWidget(m_commitButton);

    mainLayout->addLayout(buttonLayout);

    // === 信号连接 ===
    connect(m_messageEdit, &QTextEdit::textChanged, this, &GitCommitDialog::onMessageChanged);
    connect(m_amendCheckBox, &QCheckBox::toggled, this, &GitCommitDialog::onAmendToggled);
    connect(m_allowEmptyCheckBox, &QCheckBox::toggled, this, &GitCommitDialog::onAllowEmptyToggled);
    connect(m_changedFilesList, &QTreeWidget::itemChanged, this, &GitCommitDialog::onFileItemChanged);
    connect(m_changedFilesList, &QTreeWidget::itemSelectionChanged, this, &GitCommitDialog::onFileSelectionChanged);
    connect(m_fileFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GitCommitDialog::onFilterChanged);
    connect(m_fileSearchEdit, &QLineEdit::textChanged, this, &GitCommitDialog::onFilterChanged);
    connect(m_refreshButton, &QPushButton::clicked, this, &GitCommitDialog::onRefreshFiles);
    connect(m_stageSelectedButton, &QPushButton::clicked, this, &GitCommitDialog::onStageSelected);
    connect(m_unstageSelectedButton, &QPushButton::clicked, this, &GitCommitDialog::onUnstageSelected);
    connect(m_selectAllButton, &QPushButton::clicked, this, &GitCommitDialog::onSelectAll);
    connect(m_selectNoneButton, &QPushButton::clicked, this, &GitCommitDialog::onSelectNone);
    connect(m_cancelButton, &QPushButton::clicked, this, &GitCommitDialog::onCancelClicked);
    connect(m_commitButton, &QPushButton::clicked, this, &GitCommitDialog::onCommitClicked);
    
    qDebug() << "[GitCommitDialog] Enhanced UI setup completed";
}

void GitCommitDialog::loadChangedFiles()
{
    // 防止递归更新
    if (m_isUpdating) {
        qDebug() << "[GitCommitDialog] Update already in progress, skipping";
        return;
    }
    
    m_isUpdating = true;
    
    // 防止在更新过程中触发信号导致递归调用
    m_changedFilesList->blockSignals(true);
    
    m_changedFilesList->clear();
    m_files.clear();
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    // 获取所有变更的文件 (包括已暂存、已修改、未跟踪)
    process.start("git", QStringList() << "status" << "--porcelain");
    if (process.waitForFinished(5000)) {
        const QString output = QString::fromUtf8(process.readAllStandardOutput());
        const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        
        int stagedCount = 0, modifiedCount = 0, untrackedCount = 0;
        
        for (const QString &line : lines) {
            if (line.length() > 3) {
                const QString indexStatus = line.mid(0, 1);  // 暂存区状态
                const QString workingStatus = line.mid(1, 1); // 工作区状态
                const QString filePath = line.mid(3);
                
                auto *item = new QTreeWidgetItem();
                item->setText(0, QFileInfo(filePath).fileName());  // 文件名
                item->setText(2, filePath);  // 完整路径
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                
                QString status;
                QIcon icon;
                bool isStaged = false;
                
                // 根据git status输出确定文件状态
                if (indexStatus != " " && indexStatus != "?") {
                    // 文件已暂存
                    isStaged = true;
                    stagedCount++;
                    item->setCheckState(0, Qt::Checked);
                    
                    if (indexStatus == "A") {
                        status = tr("Staged (Added)");
                        icon = QIcon::fromTheme("list-add");
                    } else if (indexStatus == "M") {
                        status = tr("Staged (Modified)");
                        icon = QIcon::fromTheme("document-edit");
                    } else if (indexStatus == "D") {
                        status = tr("Staged (Deleted)");
                        icon = QIcon::fromTheme("list-remove");
                    } else if (indexStatus == "R") {
                        status = tr("Staged (Renamed)");
                        icon = QIcon::fromTheme("edit-rename");
                    } else if (indexStatus == "C") {
                        status = tr("Staged (Copied)");
                        icon = QIcon::fromTheme("edit-copy");
                    } else {
                        status = tr("Staged (%1)").arg(indexStatus);
                        icon = QIcon::fromTheme("document-properties");
                    }
                } else if (workingStatus == "?") {
                    // 未跟踪文件
                    untrackedCount++;
                    item->setCheckState(0, Qt::Unchecked);
                    status = tr("Untracked");
                    icon = QIcon::fromTheme("document-new");
                } else {
                    // 已修改但未暂存
                    modifiedCount++;
                    item->setCheckState(0, Qt::Unchecked);
                    
                    if (workingStatus == "M") {
                        status = tr("Modified");
                        icon = QIcon::fromTheme("document-edit");
                    } else if (workingStatus == "D") {
                        status = tr("Deleted");
                        icon = QIcon::fromTheme("list-remove");
                    } else {
                        status = tr("Changed (%1)").arg(workingStatus);
                        icon = QIcon::fromTheme("document-properties");
                    }
                }
                
                item->setText(1, status);
                item->setIcon(0, icon);
                item->setIcon(1, icon);
                item->setData(0, Qt::UserRole, isStaged);  // 存储是否已暂存的状态
                item->setData(1, Qt::UserRole, filePath);   // 存储完整路径
                item->setToolTip(0, tr("File: %1\nStatus: %2\nPath: %3").arg(QFileInfo(filePath).fileName(), status, filePath));
                
                m_changedFilesList->addTopLevelItem(item);
                
                if (isStaged) {
                    m_files.append(filePath);
                }
            }
        }
        
        // 更新统计标签
        updateFileCountLabels(stagedCount, modifiedCount, untrackedCount);
        
    } else {
        qWarning() << "[GitCommitDialog] Failed to load changed files:" << process.errorString();
        updateFileCountLabels(0, 0, 0);
    }
    
    // 恢复信号处理
    m_changedFilesList->blockSignals(false);
    
    // 应用当前过滤器
    applyFileFilter();
    updateButtonStates();
    
    qDebug() << "[GitCommitDialog] Loaded" << m_changedFilesList->topLevelItemCount() << "changed files";
    
    m_isUpdating = false;
}

void GitCommitDialog::updateFileCountLabels(int stagedCount, int modifiedCount, int untrackedCount)
{
    m_stagedCountLabel->setText(tr("Staged: %1").arg(stagedCount));
    m_stagedCountLabel->setStyleSheet(stagedCount > 0 ? "color: #4CAF50; font-size: 11px;" : "color: #666; font-size: 11px;");
    
    m_modifiedCountLabel->setText(tr("Modified: %1").arg(modifiedCount));
    m_modifiedCountLabel->setStyleSheet(modifiedCount > 0 ? "color: #FF9800; font-size: 11px;" : "color: #666; font-size: 11px;");
    
    m_untrackedCountLabel->setText(tr("Untracked: %1").arg(untrackedCount));
    m_untrackedCountLabel->setStyleSheet(untrackedCount > 0 ? "color: #2196F3; font-size: 11px;" : "color: #666; font-size: 11px;");
}

void GitCommitDialog::updateButtonStates()
{
    const bool hasSelection = !m_changedFilesList->selectedItems().isEmpty();
    const bool hasStaged = m_files.size() > 0;
    const bool hasMessage = !m_messageEdit->toPlainText().trimmed().isEmpty();
    
    m_stageSelectedButton->setEnabled(hasSelection);
    m_unstageSelectedButton->setEnabled(hasSelection);
    
    // 提交按钮启用条件：有暂存文件或允许空提交，且有提交消息
    m_commitButton->setEnabled((hasStaged || m_isAllowEmpty) && hasMessage);
}

void GitCommitDialog::applyFileFilter()
{
    const QString filterType = m_fileFilterCombo->currentData().toString();
    const QString searchText = m_fileSearchEdit->text().trimmed().toLower();
    
    for (int i = 0; i < m_changedFilesList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_changedFilesList->topLevelItem(i);
        const QString fileName = item->text(0).toLower();
        const QString filePath = item->text(2).toLower();
        const QString status = item->text(1);
        const bool isStaged = item->data(0, Qt::UserRole).toBool();
        
        bool matchesFilter = true;
        if (filterType == "staged") {
            matchesFilter = isStaged;
        } else if (filterType == "modified") {
            matchesFilter = !isStaged && !status.contains("Untracked");
        } else if (filterType == "untracked") {
            matchesFilter = status.contains("Untracked");
        }
        // "all" 情况下 matchesFilter 保持 true
        
        bool matchesSearch = searchText.isEmpty() || 
                           fileName.contains(searchText) || 
                           filePath.contains(searchText);
        
        item->setHidden(!(matchesFilter && matchesSearch));
    }
}

QString GitCommitDialog::getFileStatus(const QString &filePath) const
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", QStringList() << "status" << "--porcelain" << filePath);
    
    if (process.waitForFinished(3000)) {
        const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) {
            return output.left(2);  // 返回状态代码
        }
    }
    
    return QString();
}

QIcon GitCommitDialog::getStatusIcon(const QString &status) const
{
    if (status.startsWith('A')) {
        return QIcon::fromTheme("list-add");
    } else if (status.startsWith('M')) {
        return QIcon::fromTheme("document-edit");
    } else if (status.startsWith('D')) {
        return QIcon::fromTheme("list-remove");
    } else if (status.startsWith('R')) {
        return QIcon::fromTheme("edit-rename");
    } else if (status.startsWith('C')) {
        return QIcon::fromTheme("edit-copy");
    } else if (status.startsWith('?')) {
        return QIcon::fromTheme("document-new");
    } else {
        return QIcon::fromTheme("document-properties");
    }
}

void GitCommitDialog::stageFile(const QString &filePath)
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", QStringList() << "add" << filePath);
    
    if (process.waitForFinished(5000)) {
        qDebug() << "[GitCommitDialog] Successfully staged file:" << filePath;
    } else {
        qWarning() << "[GitCommitDialog] Failed to stage file:" << filePath << process.errorString();
    }
}

void GitCommitDialog::unstageFile(const QString &filePath)
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", QStringList() << "reset" << "HEAD" << filePath);
    
    if (process.waitForFinished(5000)) {
        qDebug() << "[GitCommitDialog] Successfully unstaged file:" << filePath;
    } else {
        qWarning() << "[GitCommitDialog] Failed to unstage file:" << filePath << process.errorString();
    }
}

void GitCommitDialog::loadLastCommitMessage()
{
    if (!m_isAmendMode) {
        return;
    }
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    // 获取最后一次提交的消息
    process.start("git", QStringList() << "log" << "-1" << "--pretty=format:%B");
    if (process.waitForFinished(5000)) {
        m_lastCommitMessage = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        m_messageEdit->setPlainText(m_lastCommitMessage);
        
        qDebug() << "[GitCommitDialog] Loaded last commit message for amend mode";
    } else {
        qWarning() << "[GitCommitDialog] Failed to load last commit message:" << process.errorString();
        QMessageBox::warning(this, tr("Warning"), 
                           tr("Failed to load the last commit message for amend mode."));
    }
}

QString GitCommitDialog::getCommitMessage() const
{
    return m_messageEdit->toPlainText().trimmed();
}

QStringList GitCommitDialog::getSelectedFiles() const
{
    return m_files;
}

bool GitCommitDialog::isAmendMode() const
{
    return m_isAmendMode;
}

bool GitCommitDialog::isAllowEmpty() const
{
    return m_isAllowEmpty;
}

void GitCommitDialog::onMessageChanged()
{
    updateButtonStates();
}

void GitCommitDialog::onAmendToggled(bool enabled)
{
    m_isAmendMode = enabled;
    
    if (enabled) {
        // 加载最后一次提交的消息
        loadLastCommitMessage();
        m_messageEdit->setText(m_lastCommitMessage);
    } else {
        // 清空消息编辑器
        m_messageEdit->clear();
    }
    
    updateButtonStates();
    qDebug() << "[GitCommitDialog] Amend mode:" << (enabled ? "enabled" : "disabled");
}

void GitCommitDialog::onAllowEmptyToggled(bool enabled)
{
    m_isAllowEmpty = enabled;
    
    if (enabled) {
        // 更新状态标签提示
        m_stagedCountLabel->setText(tr("Empty commit allowed. No staged files required."));
        m_stagedCountLabel->setStyleSheet("color: #FF9800; font-size: 11px;");
    } else {
        // 重新加载文件列表以更新正确的计数
        loadChangedFiles();
    }
    
    updateButtonStates();
    qDebug() << "[GitCommitDialog] Allow empty commit:" << (enabled ? "enabled" : "disabled");
}

bool GitCommitDialog::validateCommitMessage()
{
    const QString message = getCommitMessage();
    
    if (message.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Message"), 
                           tr("Please enter a commit message."));
        m_messageEdit->setFocus();
        return false;
    }
    
    if (message.length() < 3) {
        QMessageBox::warning(this, tr("Message Too Short"), 
                           tr("Commit message should be at least 3 characters long."));
        m_messageEdit->setFocus();
        return false;
    }
    
    // 检查是否有暂存文件（除非允许空提交）
    if (!m_isAllowEmpty && m_files.isEmpty()) {
        QMessageBox::warning(this, tr("No Staged Files"), 
                           tr("There are no staged files to commit.\nStage files first using git add, or enable 'Allow empty commit'."));
        return false;
    }
    
    return true;
}

void GitCommitDialog::commitChanges()
{
    if (!validateCommitMessage()) {
        return;
    }
    
    const QString message = getCommitMessage();
    
    // 构建Git命令参数
    QStringList args;
    args << "commit" << "-m" << message;
    
    if (m_isAmendMode) {
        args << "--amend";
    }
    
    if (m_isAllowEmpty) {
        args << "--allow-empty";
    }
    
    qDebug() << "[GitCommitDialog] Executing commit with args:" << args;
    
    // 使用GitOperationDialog执行提交
    auto *operationDialog = new GitOperationDialog("Commit", this);
    operationDialog->setAttribute(Qt::WA_DeleteOnClose);
    operationDialog->setOperationDescription(tr("Committing changes to repository..."));
    
    connect(operationDialog, &QDialog::accepted, [this, operationDialog]() {
        auto result = operationDialog->getExecutionResult();
        if (result == GitCommandExecutor::Result::Success) {
            qDebug() << "[GitCommitDialog] Commit completed successfully";
            accept(); // 关闭对话框
        } else {
            qWarning() << "[GitCommitDialog] Commit failed";
            // 操作对话框会显示错误信息，这里不需要额外处理
        }
    });
    
    operationDialog->executeCommand(m_repositoryPath, args);
    operationDialog->show();
}

void GitCommitDialog::onCommitClicked()
{
    commitChanges();
}

void GitCommitDialog::onCancelClicked()
{
    reject();
}

void GitCommitDialog::onFileItemChanged(QTreeWidgetItem *item, int column)
{
    if (column == 0 && item && !m_isUpdating) {
        const QString filePath = item->data(1, Qt::UserRole).toString();
        const bool shouldStage = item->checkState(0) == Qt::Checked;
        const bool isCurrentlyStaged = item->data(0, Qt::UserRole).toBool();
        
        // 防止在处理状态变更时触发额外的信号
        m_changedFilesList->blockSignals(true);
        
        qDebug() << "[GitCommitDialog] File item changed:" << filePath 
                 << "shouldStage:" << shouldStage 
                 << "isCurrentlyStaged:" << isCurrentlyStaged;
        
        bool operationSucceeded = false;
        
        if (shouldStage && !isCurrentlyStaged) {
            stageFile(filePath);
            operationSucceeded = true;
        } else if (!shouldStage && isCurrentlyStaged) {
            unstageFile(filePath);
            operationSucceeded = true;
        }
        
        // 恢复信号
        m_changedFilesList->blockSignals(false);
        
        // 使用延迟更新避免在事件处理过程中删除item导致崩溃
        if (operationSucceeded) {
            QTimer::singleShot(0, this, [this]() {
                loadChangedFiles();
            });
        }
    }
}

void GitCommitDialog::onFileSelectionChanged()
{
    updateButtonStates();
}

void GitCommitDialog::onFilterChanged()
{
    applyFileFilter();
}

void GitCommitDialog::onRefreshFiles()
{
    loadChangedFiles();
}

void GitCommitDialog::onStageSelected()
{
    const QList<QTreeWidgetItem*> selectedItems = m_changedFilesList->selectedItems();
    for (QTreeWidgetItem *item : selectedItems) {
        const QString filePath = item->data(1, Qt::UserRole).toString();
        const bool isStaged = item->data(0, Qt::UserRole).toBool();
        
        if (!isStaged) {
            stageFile(filePath);
        }
    }
    
    // 使用延迟更新避免崩溃
    QTimer::singleShot(0, this, [this]() {
        loadChangedFiles();
    });
}

void GitCommitDialog::onUnstageSelected()
{
    const QList<QTreeWidgetItem*> selectedItems = m_changedFilesList->selectedItems();
    for (QTreeWidgetItem *item : selectedItems) {
        const QString filePath = item->data(1, Qt::UserRole).toString();
        const bool isStaged = item->data(0, Qt::UserRole).toBool();
        
        if (isStaged) {
            unstageFile(filePath);
        }
    }
    
    // 使用延迟更新避免崩溃
    QTimer::singleShot(0, this, [this]() {
        loadChangedFiles();
    });
}

void GitCommitDialog::onSelectAll()
{
    for (int i = 0; i < m_changedFilesList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_changedFilesList->topLevelItem(i);
        if (!item->isHidden()) {
            item->setSelected(true);
        }
    }
}

void GitCommitDialog::onSelectNone()
{
    m_changedFilesList->clearSelection();
}

void GitCommitDialog::refreshFilesList()
{
    // 重新计算已暂存的文件列表
    m_files.clear();
    
    for (int i = 0; i < m_changedFilesList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_changedFilesList->topLevelItem(i);
        const bool isStaged = item->data(0, Qt::UserRole).toBool();
        
        if (isStaged) {
            const QString filePath = item->data(1, Qt::UserRole).toString();
            m_files.append(filePath);
        }
    }
    
    updateButtonStates();
    qDebug() << "[GitCommitDialog] Refreshed files list, staged files count:" << m_files.size();
} 