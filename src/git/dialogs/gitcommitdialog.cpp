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

GitCommitDialog::GitCommitDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent)
    , m_repositoryPath(repositoryPath)
    , m_isAmendMode(false)
    , m_isAllowEmpty(false)
    , m_optionsGroup(nullptr)
    , m_amendCheckBox(nullptr)
    , m_allowEmptyCheckBox(nullptr)
    , m_optionsLabel(nullptr)
    , m_messageGroup(nullptr)
    , m_messageEdit(nullptr)
    , m_messageHintLabel(nullptr)
    , m_filesGroup(nullptr)
    , m_fileList(nullptr)
    , m_filesCountLabel(nullptr)
    , m_commitButton(nullptr)
    , m_cancelButton(nullptr)
{
    setWindowTitle(tr("Git Commit"));
    setModal(true);
    setMinimumSize(700, 600);
    setAttribute(Qt::WA_DeleteOnClose);
    
    setupUI();
    loadStagedFiles();
    
    qDebug() << "[GitCommitDialog] Initialized for repository:" << repositoryPath;
}

GitCommitDialog::GitCommitDialog(const QString &repositoryPath, const QStringList &files, QWidget *parent)
    : GitCommitDialog(repositoryPath, parent)
{
    m_files = files;
    refreshStagedFilesDisplay();
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
    
    mainLayout->addWidget(m_messageGroup);

    // === 暂存文件区域 ===
    m_filesGroup = new QGroupBox(tr("Staged Files"), this);
    auto *filesLayout = new QVBoxLayout(m_filesGroup);
    
    m_filesCountLabel = new QLabel(this);
    m_filesCountLabel->setStyleSheet("color: #666; font-size: 11px;");
    filesLayout->addWidget(m_filesCountLabel);
    
    m_fileList = new QListWidget(this);
    m_fileList->setAlternatingRowColors(true);
    m_fileList->setSelectionMode(QAbstractItemView::NoSelection);
    filesLayout->addWidget(m_fileList);
    
    mainLayout->addWidget(m_filesGroup);

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
    connect(m_cancelButton, &QPushButton::clicked, this, &GitCommitDialog::onCancelClicked);
    connect(m_commitButton, &QPushButton::clicked, this, &GitCommitDialog::onCommitClicked);
    
    qDebug() << "[GitCommitDialog] UI setup completed";
}

void GitCommitDialog::loadStagedFiles()
{
    m_fileList->clear();
    m_files.clear();
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    // 获取暂存区文件
    process.start("git", QStringList() << "diff" << "--cached" << "--name-status");
    if (process.waitForFinished(5000)) {
        const QString output = QString::fromUtf8(process.readAllStandardOutput());
        const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        
        for (const QString &line : lines) {
            if (line.length() > 2) {
                const QString status = line.left(1);
                const QString filePath = line.mid(2);
                m_files.append(filePath);
                
                auto *item = new QListWidgetItem();
                QString statusText;
                QIcon icon;
                
                if (status == "A") {
                    statusText = tr("Added");
                    icon = QIcon::fromTheme("list-add");
                } else if (status == "M") {
                    statusText = tr("Modified");
                    icon = QIcon::fromTheme("document-edit");
                } else if (status == "D") {
                    statusText = tr("Deleted");
                    icon = QIcon::fromTheme("list-remove");
                } else {
                    statusText = status;
                    icon = QIcon::fromTheme("document-properties");
                }
                
                item->setText(QString("%1 - %2").arg(filePath, statusText));
                item->setIcon(icon);
                item->setToolTip(tr("File: %1\nStatus: %2").arg(filePath, statusText));
                
                m_fileList->addItem(item);
            }
        }
    } else {
        qWarning() << "[GitCommitDialog] Failed to load staged files:" << process.errorString();
    }
    
    refreshStagedFilesDisplay();
}

void GitCommitDialog::refreshStagedFilesDisplay()
{
    const int fileCount = m_files.size();
    if (fileCount == 0) {
        m_filesCountLabel->setText(tr("No files staged for commit. Stage files first using git add."));
        m_filesCountLabel->setStyleSheet("color: #FF6B35; font-size: 11px;");
        m_commitButton->setEnabled(false);
    } else {
        m_filesCountLabel->setText(tr("%1 file(s) staged for commit:").arg(fileCount));
        m_filesCountLabel->setStyleSheet("color: #4CAF50; font-size: 11px;");
    }
    
    // 更新提交按钮状态
    onMessageChanged();
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
    const QString message = getCommitMessage();
    const bool hasMessage = !message.isEmpty();
    const bool hasFiles = !m_files.isEmpty();
    
    // 启用提交按钮的条件：
    // 1. 有提交消息 AND (有暂存文件 OR 允许空提交)
    const bool canCommit = hasMessage && (hasFiles || m_isAllowEmpty);
    
    m_commitButton->setEnabled(canCommit);
    
    // 更新按钮文本
    if (m_isAmendMode) {
        m_commitButton->setText(tr("Amend Commit"));
    } else {
        m_commitButton->setText(tr("Commit"));
    }
}

void GitCommitDialog::onAmendToggled(bool enabled)
{
    m_isAmendMode = enabled;
    
    if (enabled) {
        loadLastCommitMessage();
        m_messageHintLabel->setText(tr("Modifying the last commit. Edit the message as needed:"));
        m_messageHintLabel->setStyleSheet("color: #FF9800; font-size: 11px; font-weight: bold;");
    } else {
        m_messageEdit->clear();
        m_messageHintLabel->setText(tr("Enter a clear and descriptive commit message:"));
        m_messageHintLabel->setStyleSheet("color: #666; font-size: 11px;");
    }
    
    onMessageChanged();
}

void GitCommitDialog::onAllowEmptyToggled(bool enabled)
{
    m_isAllowEmpty = enabled;
    
    if (enabled) {
        m_filesCountLabel->setText(tr("Empty commit allowed. No staged files required."));
        m_filesCountLabel->setStyleSheet("color: #FF9800; font-size: 11px;");
    } else {
        refreshStagedFilesDisplay();
    }
    
    onMessageChanged();
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