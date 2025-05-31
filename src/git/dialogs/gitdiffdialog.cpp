#include "gitdiffdialog.h"
#include "../utils.h"
#include "../gitstatusparser.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QListWidget>
#include <QSplitter>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QDebug>
#include <QSizePolicy>

GitDiffDialog::GitDiffDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
    : QDialog(parent), 
      m_repositoryPath(repositoryPath), 
      m_filePath(filePath),
      m_isDirectory(QFileInfo(filePath).isDir()),
      m_splitter(nullptr),
      m_fileListWidget(nullptr),
      m_diffView(nullptr),
      m_refreshButton(nullptr),
      m_fileInfoLabel(nullptr)
{
    qDebug() << "[GitDiffDialog] Initializing dialog for path:" << filePath 
             << "in repository:" << repositoryPath 
             << "mode:" << (m_isDirectory ? "directory" : "single file");
    
    setupUI();
    loadFileDiff();
    
    qDebug() << "[GitDiffDialog] Dialog initialization completed successfully";
}

void GitDiffDialog::setupUI()
{
    if (m_isDirectory) {
        setWindowTitle(tr("Git Diff - %1 (Directory)").arg(QFileInfo(m_filePath).fileName()));
        setupDirectoryUI();
    } else {
        setWindowTitle(tr("Git Diff - %1").arg(QFileInfo(m_filePath).fileName()));
        setupSingleFileUI();
    }
    
    setModal(false);
    resize(1200, 700);
}

void GitDiffDialog::setupSingleFileUI()
{
    auto *layout = new QVBoxLayout(this);

    // 文件信息标签
    m_fileInfoLabel = new QLabel;
    m_fileInfoLabel->setWordWrap(true);
    m_fileInfoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 8px; border-radius: 4px; }");
    layout->addWidget(m_fileInfoLabel);

    // 工具栏
    auto *toolbarLayout = new QHBoxLayout;
    m_refreshButton = new QPushButton(tr("Refresh"));
    connect(m_refreshButton, &QPushButton::clicked, this, &GitDiffDialog::onRefreshClicked);
    toolbarLayout->addWidget(m_refreshButton);
    toolbarLayout->addStretch();
    layout->addLayout(toolbarLayout);

    // 差异视图
    m_diffView = new QTextEdit;
    m_diffView->setReadOnly(true);
    m_diffView->setFont(QFont("Consolas", 10));
    m_diffView->setLineWrapMode(QTextEdit::NoWrap);
    layout->addWidget(m_diffView);

    // 关闭按钮
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    auto *closeButton = new QPushButton(tr("Close"));
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
}

void GitDiffDialog::setupDirectoryUI()
{
    auto *layout = new QVBoxLayout(this);

    // 文件信息标签 - 压缩高度
    m_fileInfoLabel = new QLabel;
    m_fileInfoLabel->setWordWrap(true);
    m_fileInfoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 4px 8px; border-radius: 4px; }");
    m_fileInfoLabel->setMaximumHeight(60);  // 限制最大高度
    m_fileInfoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(m_fileInfoLabel);

    // 工具栏 - 压缩高度
    auto *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setContentsMargins(0, 2, 0, 2);  // 减少边距
    m_refreshButton = new QPushButton(tr("Refresh"));
    m_refreshButton->setMaximumHeight(28);  // 限制按钮高度
    connect(m_refreshButton, &QPushButton::clicked, this, &GitDiffDialog::onRefreshClicked);
    toolbarLayout->addWidget(m_refreshButton);
    toolbarLayout->addStretch();
    layout->addLayout(toolbarLayout);

    // 主要内容区域 - 使用分割器
    m_splitter = new QSplitter(Qt::Horizontal, this);
    
    // 左侧文件列表
    m_fileListWidget = new QListWidget;
    m_fileListWidget->setMinimumWidth(250);
    m_fileListWidget->setMaximumWidth(400);
    connect(m_fileListWidget, &QListWidget::itemClicked, this, &GitDiffDialog::onFileItemClicked);
    m_splitter->addWidget(m_fileListWidget);

    // 右侧差异视图
    m_diffView = new QTextEdit;
    m_diffView->setReadOnly(true);
    m_diffView->setFont(QFont("Consolas", 10));
    m_diffView->setLineWrapMode(QTextEdit::NoWrap);
    m_diffView->setPlainText(tr("Select a file from the list to view its changes."));
    m_splitter->addWidget(m_diffView);

    // 设置分割器比例
    m_splitter->setStretchFactor(0, 0);  // 文件列表固定宽度
    m_splitter->setStretchFactor(1, 1);  // 差异视图占据剩余空间

    layout->addWidget(m_splitter);

    // 关闭按钮
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 2, 0, 0);  // 减少边距
    buttonLayout->addStretch();
    auto *closeButton = new QPushButton(tr("Close"));
    closeButton->setMaximumHeight(28);  // 限制按钮高度
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
}

void GitDiffDialog::loadFileDiff()
{
    if (m_isDirectory) {
        loadDirectoryDiff();
    } else {
        loadSingleFileDiff(m_filePath);
    }
}

void GitDiffDialog::loadDirectoryDiff()
{
    // 获取目录下所有有变化的文件
    populateFileList();
    
    // 更新文件信息标签
    QDir repoDir(m_repositoryPath);
    QString relativePath = repoDir.relativeFilePath(m_filePath);
    QString branchName = Utils::getBranchName(m_repositoryPath);
    
    m_fileInfoLabel->setText(tr("Directory: %1\nBranch: %2\nRepository: %3\nChanged files: %4")
                                     .arg(relativePath, branchName, m_repositoryPath)
                                     .arg(m_changedFiles.size()));
}

void GitDiffDialog::loadSingleFileDiff(const QString &filePath)
{
    qDebug() << "[GitDiffDialog::loadSingleFileDiff] Loading diff for file:" << filePath;
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    // 计算相对路径
    QDir repoDir(m_repositoryPath);
    QString relativePath = repoDir.relativeFilePath(filePath);

    QStringList args { "diff", "HEAD", "--", relativePath };
    process.start("git", args);

    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QString error = QString::fromUtf8(process.readAllStandardError());

        if (process.exitCode() == 0) {
            if (output.isEmpty()) {
                qDebug() << "[GitDiffDialog::loadSingleFileDiff] No changes found for file:" << relativePath;
                m_diffView->setPlainText(tr("No changes found in this file."));
            } else {
                qDebug() << "[GitDiffDialog::loadSingleFileDiff] Successfully loaded diff for file:" << relativePath;
                m_diffView->setPlainText(output);
                applySyntaxHighlighting();
            }

            // 更新文件信息（仅在单文件模式下）
            if (!m_isDirectory) {
                QString statusText = Utils::getFileStatusDescription(filePath);
                QString branchName = Utils::getBranchName(m_repositoryPath);
                m_fileInfoLabel->setText(tr("File: %1\nStatus: %2\nBranch: %3\nRepository: %4")
                                                 .arg(relativePath, statusText, branchName, m_repositoryPath));
            }
        } else {
            qWarning() << "[GitDiffDialog::loadSingleFileDiff] Git diff command failed for file:" 
                       << relativePath << "Error:" << error;
            m_diffView->setPlainText(tr("Error loading diff:\n%1").arg(error));
        }
    } else {
        qCritical() << "[GitDiffDialog::loadSingleFileDiff] Git diff command timeout for file:" << relativePath;
        m_diffView->setPlainText(tr("Failed to execute git diff command."));
    }
}

void GitDiffDialog::populateFileList()
{
    qDebug() << "[GitDiffDialog::populateFileList] Populating file list for directory:" << m_filePath;
    
    m_fileListWidget->clear();
    m_changedFiles.clear();

    // 获取仓库状态
    auto fileInfoList = GitStatusParser::getRepositoryStatus(m_repositoryPath);
    
    qDebug() << "[GitDiffDialog::populateFileList] Retrieved" << fileInfoList.size() 
             << "files from repository status";
    
    QDir repoDir(m_repositoryPath);
    QString targetRelativePath = repoDir.relativeFilePath(m_filePath);
    
    // 过滤出目标目录下的文件
    for (const auto &fileInfo : fileInfoList) {
        QString absolutePath = repoDir.absoluteFilePath(fileInfo->filePath);
        
        // 检查文件是否在目标目录下
        bool isInTargetDir = false;
        if (m_filePath == m_repositoryPath) {
            // 如果目标是仓库根目录，显示所有文件
            isInTargetDir = true;
        } else {
            // 检查文件是否在目标目录下（包括子目录）
            QString normalizedTargetPath = QDir::cleanPath(m_filePath);
            QString normalizedFilePath = QDir::cleanPath(absolutePath);
            isInTargetDir = normalizedFilePath.startsWith(normalizedTargetPath + "/");
        }
        
        if (isInTargetDir) {
            m_changedFiles.append(absolutePath);
            
            // 创建列表项
            auto *item = new QListWidgetItem(m_fileListWidget);
            
            // 显示相对于目标目录的路径
            QString displayPath;
            if (m_filePath == m_repositoryPath) {
                displayPath = fileInfo->filePath;
            } else {
                QDir targetDir(m_filePath);
                displayPath = targetDir.relativeFilePath(absolutePath);
            }
            
            item->setText(displayPath);
            item->setData(Qt::UserRole, absolutePath);  // 存储完整路径
            item->setIcon(fileInfo->statusIcon());
            item->setToolTip(tr("%1\nStatus: %2").arg(displayPath, fileInfo->statusDisplayText()));
        }
    }
    
    // 如果没有找到变化的文件，显示提示信息
    if (m_changedFiles.isEmpty()) {
        qDebug() << "[GitDiffDialog::populateFileList] No changed files found in directory:" << m_filePath;
        auto *item = new QListWidgetItem(tr("No changed files found in this directory"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        m_fileListWidget->addItem(item);
        m_diffView->setPlainText(tr("No changed files found in this directory."));
    } else {
        qDebug() << "[GitDiffDialog::populateFileList] Successfully populated" << m_changedFiles.size() 
                 << "files in list";
        
        // 默认选中第一个文件并显示其diff
        if (m_fileListWidget->count() > 0) {
            m_fileListWidget->setCurrentRow(0);
            QListWidgetItem *firstItem = m_fileListWidget->item(0);
            if (firstItem && firstItem->data(Qt::UserRole).isValid()) {
                QString firstFilePath = firstItem->data(Qt::UserRole).toString();
                m_currentSelectedFile = firstFilePath;
                qDebug() << "[GitDiffDialog::populateFileList] Auto-selecting first file:" << firstFilePath;
                loadSingleFileDiff(firstFilePath);
            }
        }
    }
}

void GitDiffDialog::applySyntaxHighlighting()
{
    // 简单的差异语法高亮
    QTextDocument *doc = m_diffView->document();
    QTextCursor cursor(doc);

    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);

    QTextCharFormat addedFormat;
    addedFormat.setBackground(QColor(220, 255, 220));

    QTextCharFormat removedFormat;
    removedFormat.setBackground(QColor(255, 220, 220));

    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor(128, 128, 128));
    headerFormat.setFontWeight(QFont::Bold);

    while (!cursor.atEnd()) {
        cursor.select(QTextCursor::LineUnderCursor);
        QString line = cursor.selectedText();

        if (line.startsWith('+') && !line.startsWith("+++")) {
            cursor.setCharFormat(addedFormat);
        } else if (line.startsWith('-') && !line.startsWith("---")) {
            cursor.setCharFormat(removedFormat);
        } else if (line.startsWith("@@") || line.startsWith("diff ") || line.startsWith("index ") || line.startsWith("+++") || line.startsWith("---")) {
            cursor.setCharFormat(headerFormat);
        }

        cursor.movePosition(QTextCursor::NextBlock);
    }

    cursor.endEditBlock();
}

void GitDiffDialog::onRefreshClicked()
{
    loadFileDiff();
}

void GitDiffDialog::onFileItemClicked(QListWidgetItem *item)
{
    if (!item || !item->data(Qt::UserRole).isValid()) {
        return;
    }
    
    QString filePath = item->data(Qt::UserRole).toString();
    m_currentSelectedFile = filePath;
    
    // 加载选中文件的差异
    loadSingleFileDiff(filePath);
}

QString GitDiffDialog::getRelativePath(const QString &absolutePath) const
{
    QDir repoDir(m_repositoryPath);
    return repoDir.relativeFilePath(absolutePath);
} 