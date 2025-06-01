#include "gitstatusdialog.h"
#include "gitdialogs.h"
#include "gitstatusparser.h"
#include "gitoperationutils.h"
#include "widgets/linenumbertextedit.h"
#include "gitfilepreviewdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QProcess>
#include <QMessageBox>
#include <QApplication>
#include <QProgressDialog>
#include <QDebug>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QClipboard>
#include <QTimer>
#include <QTextDocument>
#include <QTextCharFormat>
#include <QSyntaxHighlighter>
#include <QGroupBox>
#include <QKeyEvent>
#include <QEvent>

/**
 * @brief Git差异语法高亮器
 * 为diff输出提供颜色高亮，增强可读性
 */
class GitDiffHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit GitDiffHighlighter(QTextDocument *parent = nullptr)
        : QSyntaxHighlighter(parent)
    {
        // 设置添加行格式（绿色）
        m_addedFormat.setForeground(QColor(0, 128, 0));
        m_addedFormat.setBackground(QColor(220, 255, 220));

        // 设置删除行格式（红色）
        m_removedFormat.setForeground(QColor(128, 0, 0));
        m_removedFormat.setBackground(QColor(255, 220, 220));

        // 设置文件头格式（蓝色）
        m_headerFormat.setForeground(QColor(0, 0, 128));
        m_headerFormat.setFontWeight(QFont::Bold);

        // 设置行号格式（灰色）
        m_lineNumberFormat.setForeground(QColor(128, 128, 128));
    }

protected:
    void highlightBlock(const QString &text) override
    {
        // 根据行的开头字符进行高亮
        if (text.startsWith('+') && !text.startsWith("+++")) {
            setFormat(0, text.length(), m_addedFormat);
        } else if (text.startsWith('-') && !text.startsWith("---")) {
            setFormat(0, text.length(), m_removedFormat);
        } else if (text.startsWith("@@") || text.startsWith("+++") || text.startsWith("---") || text.startsWith("diff ")) {
            setFormat(0, text.length(), m_headerFormat);
        } else if (text.startsWith("\\")) {
            setFormat(0, text.length(), m_lineNumberFormat);
        }
    }

private:
    QTextCharFormat m_addedFormat;
    QTextCharFormat m_removedFormat;
    QTextCharFormat m_headerFormat;
    QTextCharFormat m_lineNumberFormat;
};

GitStatusDialog::GitStatusDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_branchLabel(nullptr), m_statusSummary(nullptr), m_mainSplitter(nullptr), m_listSplitter(nullptr), m_workingTreeWidget(nullptr), m_stagingAreaWidget(nullptr), m_diffPreviewWidget(nullptr), m_refreshButton(nullptr), m_stageSelectedBtn(nullptr), m_unstageSelectedBtn(nullptr), m_addSelectedBtn(nullptr), m_resetSelectedBtn(nullptr), m_commitBtn(nullptr), m_contextMenu(nullptr), m_currentPreviewDialog(nullptr)
{
    setWindowTitle(tr("Git Repository Status"));
    setMinimumSize(1000, 700);
    // Enable maximize button and window resizing
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    // Set initial size larger to better utilize screen space
    resize(1400, 900);
    setAttribute(Qt::WA_DeleteOnClose);
    setupUI();
    setupContextMenu();
    loadRepositoryStatus();
    updateButtonStates();

    // 安装事件过滤器来捕获TreeWidget的键盘事件
    m_workingTreeWidget->installEventFilter(this);
    m_stagingAreaWidget->installEventFilter(this);

    qDebug() << "[GitStatusDialog] Initialized with enhanced features for repository:" << repositoryPath;
}

void GitStatusDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // === 仓库信息区域 ===
    auto *infoGroup = new QGroupBox(tr("Repository Information"), this);
    infoGroup->setMaximumHeight(80);   // 限制最大高度，保持紧凑
    auto *infoLayout = new QHBoxLayout(infoGroup);   // 使用水平布局更紧凑
    infoLayout->setContentsMargins(8, 4, 8, 4);

    auto *infoLeftLayout = new QVBoxLayout();
    infoLeftLayout->setSpacing(2);

    m_branchLabel = new QLabel(this);
    m_branchLabel->setStyleSheet("font-weight: bold; color: #2196F3; font-size: 12px;");
    infoLeftLayout->addWidget(m_branchLabel);

    m_statusSummary = new QLabel(this);
    m_statusSummary->setWordWrap(true);
    m_statusSummary->setStyleSheet("color: #666; font-size: 11px;");
    infoLeftLayout->addWidget(m_statusSummary);

    infoLayout->addLayout(infoLeftLayout);
    infoLayout->addStretch();   // 添加弹性空间

    mainLayout->addWidget(infoGroup);

    // === 主分割器：文件列表 + 差异预览 ===
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // === 左侧：文件列表区域 ===
    auto *filesWidget = new QWidget();
    auto *filesLayout = new QVBoxLayout(filesWidget);
    filesLayout->setContentsMargins(0, 0, 0, 0);

    // 文件列表分割器（垂直分割工作区和暂存区）
    m_listSplitter = new QSplitter(Qt::Vertical, this);

    // 工作区文件列表
    auto *workingGroup = new QGroupBox(tr("Working Directory Files"), this);
    auto *workingLayout = new QVBoxLayout(workingGroup);

    m_workingTreeWidget = new QTreeWidget(this);
    m_workingTreeWidget->setHeaderLabels({ tr("File"), tr("Status") });
    m_workingTreeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_workingTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_workingTreeWidget->setAlternatingRowColors(true);
    m_workingTreeWidget->header()->setStretchLastSection(false);
    m_workingTreeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_workingTreeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    workingLayout->addWidget(m_workingTreeWidget);

    m_listSplitter->addWidget(workingGroup);

    // 暂存区文件列表
    auto *stagingGroup = new QGroupBox(tr("Staging Area Files"), this);
    auto *stagingLayout = new QVBoxLayout(stagingGroup);

    m_stagingAreaWidget = new QTreeWidget(this);
    m_stagingAreaWidget->setHeaderLabels({ tr("File"), tr("Status") });
    m_stagingAreaWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_stagingAreaWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_stagingAreaWidget->setAlternatingRowColors(true);
    m_stagingAreaWidget->header()->setStretchLastSection(false);
    m_stagingAreaWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_stagingAreaWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    stagingLayout->addWidget(m_stagingAreaWidget);

    m_listSplitter->addWidget(stagingGroup);

    // 设置列表分割器比例
    m_listSplitter->setSizes({ 350, 200 });
    filesLayout->addWidget(m_listSplitter);

    // 快捷操作按钮
    auto *buttonLayout = new QHBoxLayout();

    m_addSelectedBtn = new QPushButton(tr("Add Selected"), this);
    m_addSelectedBtn->setToolTip(tr("Add selected files to Git tracking"));
    buttonLayout->addWidget(m_addSelectedBtn);

    m_stageSelectedBtn = new QPushButton(tr("Stage Selected"), this);
    m_stageSelectedBtn->setToolTip(tr("Stage selected files for commit"));
    buttonLayout->addWidget(m_stageSelectedBtn);

    m_unstageSelectedBtn = new QPushButton(tr("Unstage Selected"), this);
    m_unstageSelectedBtn->setToolTip(tr("Remove selected files from staging area"));
    buttonLayout->addWidget(m_unstageSelectedBtn);

    m_resetSelectedBtn = new QPushButton(tr("Reset Selected"), this);
    m_resetSelectedBtn->setToolTip(tr("Discard changes in selected files"));
    buttonLayout->addWidget(m_resetSelectedBtn);

    buttonLayout->addStretch();

    m_commitBtn = new QPushButton(tr("Commit..."), this);
    m_commitBtn->setToolTip(tr("Commit staged changes"));
    buttonLayout->addWidget(m_commitBtn);

    filesLayout->addLayout(buttonLayout);
    m_mainSplitter->addWidget(filesWidget);

    // === 右侧：差异预览区域 ===
    auto *previewGroup = new QGroupBox(tr("File Diff Preview"), this);
    auto *previewLayout = new QVBoxLayout(previewGroup);

    m_diffPreviewWidget = new LineNumberTextEdit(this);
    m_diffPreviewWidget->setReadOnly(true);
    m_diffPreviewWidget->setFont(QFont("Courier", 9));
    m_diffPreviewWidget->setPlaceholderText(tr("Select a file to view its differences here..."));

    // 应用语法高亮
    auto *highlighter = new GitDiffHighlighter(m_diffPreviewWidget->document());
    Q_UNUSED(highlighter)

    previewLayout->addWidget(m_diffPreviewWidget);
    m_mainSplitter->addWidget(previewGroup);

    // 设置主分割器比例和属性
    m_mainSplitter->setSizes({ 600, 800 });   // 调整初始比例，给文件预览更多空间
    m_mainSplitter->setStretchFactor(0, 1);   // 文件列表区域可拉伸
    m_mainSplitter->setStretchFactor(1, 1);   // 预览区域也可拉伸
    m_mainSplitter->setChildrenCollapsible(false);   // 防止子窗口被完全折叠
    mainLayout->addWidget(m_mainSplitter);

    // === 底部按钮区域 ===
    auto *bottomLayout = new QHBoxLayout();

    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setToolTip(tr("Refresh repository status"));
    bottomLayout->addWidget(m_refreshButton);

    bottomLayout->addStretch();

    auto *closeButton = new QPushButton(tr("Close"), this);
    bottomLayout->addWidget(closeButton);

    mainLayout->addLayout(bottomLayout);

    // === 信号连接 ===
    connect(m_refreshButton, &QPushButton::clicked, this, &GitStatusDialog::onRefreshClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    // 文件选择变化
    connect(m_workingTreeWidget, &QTreeWidget::itemSelectionChanged,
            this, &GitStatusDialog::onFileSelectionChanged);
    connect(m_stagingAreaWidget, &QTreeWidget::itemSelectionChanged,
            this, &GitStatusDialog::onFileSelectionChanged);

    // 双击事件
    connect(m_workingTreeWidget, &QTreeWidget::itemDoubleClicked,
            this, &GitStatusDialog::onFileDoubleClicked);
    connect(m_stagingAreaWidget, &QTreeWidget::itemDoubleClicked,
            this, &GitStatusDialog::onFileDoubleClicked);

    // 右键菜单
    connect(m_workingTreeWidget, &QTreeWidget::customContextMenuRequested,
            this, &GitStatusDialog::showFileContextMenu);
    connect(m_stagingAreaWidget, &QTreeWidget::customContextMenuRequested,
            this, &GitStatusDialog::showFileContextMenu);

    // 操作按钮
    connect(m_addSelectedBtn, &QPushButton::clicked, this, &GitStatusDialog::addSelectedFiles);
    connect(m_stageSelectedBtn, &QPushButton::clicked, this, &GitStatusDialog::stageSelectedFiles);
    connect(m_unstageSelectedBtn, &QPushButton::clicked, this, &GitStatusDialog::unstageSelectedFiles);
    connect(m_resetSelectedBtn, &QPushButton::clicked, this, &GitStatusDialog::resetSelectedFiles);
    connect(m_commitBtn, &QPushButton::clicked, this, &GitStatusDialog::commitSelectedFiles);

    qDebug() << "[GitStatusDialog] UI setup completed with enhanced layout";
}

void GitStatusDialog::setupContextMenu()
{
    m_contextMenu = new QMenu(this);

    // === Git Operations ===
    m_addAction = m_contextMenu->addAction(QIcon::fromTheme("list-add"), tr("Add to Git"));
    m_stageAction = m_contextMenu->addAction(QIcon::fromTheme("go-up"), tr("Stage"));
    m_unstageAction = m_contextMenu->addAction(QIcon::fromTheme("go-down"), tr("Unstage"));
    m_contextMenu->addSeparator();

    m_revertAction = m_contextMenu->addAction(QIcon::fromTheme("edit-undo"), tr("Revert Changes"));
    m_removeAction = m_contextMenu->addAction(QIcon::fromTheme("list-remove"), tr("Remove from Git"));
    m_diffAction = m_contextMenu->addAction(QIcon::fromTheme("document-properties"), tr("View Diff"));

    m_contextMenu->addSeparator();

    // === File Preview Action ===
    m_previewAction = m_contextMenu->addAction(QIcon::fromTheme("document-preview"), tr("Preview File"));
    m_previewAction->setToolTip(tr("Quick preview file content (Space key)"));

    m_contextMenu->addSeparator();

    // === File Management Actions ===
    auto *openFileAction = m_contextMenu->addAction(QIcon::fromTheme("document-open"), tr("Open File"));
    auto *showFolderAction = m_contextMenu->addAction(QIcon::fromTheme("folder-open"), tr("Show in Folder"));

    m_contextMenu->addSeparator();

    // === Git History Actions ===
    auto *showLogAction = m_contextMenu->addAction(QIcon::fromTheme("view-list-details"), tr("Show File Log"));
    auto *showBlameAction = m_contextMenu->addAction(QIcon::fromTheme("view-list-tree"), tr("Show File Blame"));

    m_contextMenu->addSeparator();

    // === Advanced Actions ===
    auto *copyPathAction = m_contextMenu->addAction(QIcon::fromTheme("edit-copy"), tr("Copy File Path"));
    auto *copyNameAction = m_contextMenu->addAction(QIcon::fromTheme("edit-copy"), tr("Copy File Name"));
    auto *deleteFileAction = m_contextMenu->addAction(QIcon::fromTheme("edit-delete"), tr("Delete File"));

    // === Connect existing actions ===
    connect(m_addAction, &QAction::triggered, this, &GitStatusDialog::addSelectedFiles);
    connect(m_stageAction, &QAction::triggered, this, &GitStatusDialog::stageSelectedFiles);
    connect(m_unstageAction, &QAction::triggered, this, &GitStatusDialog::unstageSelectedFiles);
    connect(m_revertAction, &QAction::triggered, this, &GitStatusDialog::resetSelectedFiles);

    // Connect preview action
    connect(m_previewAction, &QAction::triggered, this, &GitStatusDialog::previewSelectedFile);

    // Updated diff action to use GitDialogManager
    connect(m_diffAction, &QAction::triggered, [this]() {
        auto selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            const QString filePath = selectedFiles.first()->text(0);
            GitDialogManager::instance()->showDiffDialog(m_repositoryPath, filePath, this);
        }
    });

    // === Connect new actions ===
    // File management actions
    connect(openFileAction, &QAction::triggered, this, [this]() {
        auto selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            QString relativePath = selectedFiles.first()->text(0);
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(relativePath);
            GitDialogManager::instance()->openFile(absolutePath, this);
        }
    });

    connect(showFolderAction, &QAction::triggered, this, [this]() {
        auto selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            QString relativePath = selectedFiles.first()->text(0);
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(relativePath);
            GitDialogManager::instance()->showFileInFolder(absolutePath, this);
        }
    });

    // Git history actions
    connect(showLogAction, &QAction::triggered, this, [this]() {
        auto selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            QString filePath = selectedFiles.first()->text(0);
            GitDialogManager::instance()->showLogDialog(m_repositoryPath, filePath, this);
        }
    });

    connect(showBlameAction, &QAction::triggered, this, [this]() {
        auto selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            QString relativePath = selectedFiles.first()->text(0);
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(relativePath);
            GitDialogManager::instance()->showBlameDialog(m_repositoryPath, absolutePath, this);
        }
    });

    // Advanced actions
    connect(copyPathAction, &QAction::triggered, this, [this]() {
        auto selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            QString relativePath = selectedFiles.first()->text(0);
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(relativePath);
            QApplication::clipboard()->setText(absolutePath);
            qDebug() << "[GitStatusDialog] Copied file path to clipboard:" << absolutePath;
        }
    });

    connect(copyNameAction, &QAction::triggered, this, [this]() {
        auto selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            QString filePath = selectedFiles.first()->text(0);
            QString fileName = QFileInfo(filePath).fileName();
            QApplication::clipboard()->setText(fileName);
            qDebug() << "[GitStatusDialog] Copied file name to clipboard:" << fileName;
        }
    });

    connect(deleteFileAction, &QAction::triggered, this, [this]() {
        auto selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            QString relativePath = selectedFiles.first()->text(0);
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(relativePath);
            GitDialogManager::instance()->deleteFile(absolutePath, this);
            // Refresh after potential deletion
            QTimer::singleShot(100, this, [this]() {
                onRefreshClicked();
            });
        }
    });
}

void GitStatusDialog::loadRepositoryStatus()
{
    if (m_repositoryPath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid repository path"));
        return;
    }

    // 清空现有内容
    m_workingTreeWidget->clear();
    m_stagingAreaWidget->clear();
    m_diffPreviewWidget->clear();

    // === 获取当前分支信息 ===
    QString currentBranch = GitOperationUtils::getCurrentBranch(m_repositoryPath);
    m_branchLabel->setText(tr("Current Branch: %1").arg(currentBranch));

    // === 使用GitStatusParser获取文件状态 ===
    auto files = GitStatusParser::getRepositoryStatus(m_repositoryPath);

    for (const auto &fileInfo : files) {
        // 构建状态字符串（兼容现有的显示逻辑）
        QString statusCode;
        bool addToStaging = false;
        bool addToWorking = false;

        switch (fileInfo->status) {
        case GitFileStatus::StagedAdded:
            statusCode = "A ";
            addToStaging = true;
            break;
        case GitFileStatus::StagedModified:
            statusCode = "M ";
            addToStaging = true;
            break;
        case GitFileStatus::StagedDeleted:
            statusCode = "D ";
            addToStaging = true;
            break;
        case GitFileStatus::Renamed:
            statusCode = "R ";
            addToStaging = true;
            break;
        case GitFileStatus::Copied:
            statusCode = "C ";
            addToStaging = true;
            break;
        case GitFileStatus::Modified:
            statusCode = " M";
            addToWorking = true;
            break;
        case GitFileStatus::Deleted:
            statusCode = " D";
            addToWorking = true;
            break;
        case GitFileStatus::Untracked:
            statusCode = "??";
            addToWorking = true;
            break;
        default:
            continue;   // 跳过未知状态
        }

        if (addToStaging) {
            // 已暂存的文件
            auto *item = new QTreeWidgetItem(m_stagingAreaWidget);
            item->setText(0, fileInfo->filePath);
            item->setText(1, GitStatusParser::getStatusDescription(statusCode));
            item->setIcon(0, GitStatusParser::getStatusIcon(statusCode));
            item->setData(0, Qt::UserRole, statusCode);
            item->setToolTip(0, tr("Status: %1\nRight-click for options").arg(GitStatusParser::getStatusDescription(statusCode)));
        }

        if (addToWorking) {
            // 工作区文件
            auto *item = new QTreeWidgetItem(m_workingTreeWidget);
            item->setText(0, fileInfo->filePath);
            item->setText(1, GitStatusParser::getStatusDescription(statusCode));
            item->setIcon(0, GitStatusParser::getStatusIcon(statusCode));
            item->setData(0, Qt::UserRole, statusCode);
            item->setToolTip(0, tr("Status: %1\nRight-click for options").arg(GitStatusParser::getStatusDescription(statusCode)));
        }
    }

    updateStatusSummary();
    updateButtonStates();

    qDebug() << "[GitStatusDialog] Repository status loaded successfully using GitStatusParser";
}

void GitStatusDialog::updateStatusSummary()
{
    const int workingCount = m_workingTreeWidget->topLevelItemCount();
    const int stagingCount = m_stagingAreaWidget->topLevelItemCount();

    QString summary;
    if (workingCount == 0 && stagingCount == 0) {
        summary = tr("Working directory is clean");
    } else {
        QStringList parts;
        if (workingCount > 0) {
            parts << tr("%1 files in working directory").arg(workingCount);
        }
        if (stagingCount > 0) {
            parts << tr("%1 files in staging area").arg(stagingCount);
        }
        summary = parts.join(", ");
    }

    m_statusSummary->setText(summary);
}

void GitStatusDialog::onFileSelectionChanged()
{
    updateButtonStates();

    // 获取选中的文件进行差异预览
    auto selectedFiles = getSelectedFiles();
    if (!selectedFiles.isEmpty()) {
        const QString filePath = selectedFiles.first()->text(0);
        const QString status = selectedFiles.first()->data(0, Qt::UserRole).toString();
        refreshDiffPreview(filePath, status);
    } else {
        m_diffPreviewWidget->clear();
    }
}

void GitStatusDialog::refreshDiffPreview(const QString &filePath, const QString &status)
{
    if (filePath.isEmpty()) {
        m_diffPreviewWidget->clear();
        return;
    }

    qDebug() << "[GitStatusDialog] Refreshing diff preview for:" << filePath << "status:" << status;

    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    // 根据状态选择合适的diff命令
    QStringList args;
    if (status == "??") {
        // 未跟踪文件 - 显示文件内容
        args << "show" << ("HEAD:" + filePath);
        m_diffPreviewWidget->setPlainText(tr("Untracked file: %1\n\nContent preview is not available for untracked files.\nUse 'Add to Git' to start tracking this file.").arg(filePath));
        return;
    } else if (status.at(0) != ' ') {
        // 暂存区文件 - 显示HEAD与暂存区的差异
        args << "diff"
             << "--cached" << filePath;
    } else {
        // 工作区文件 - 显示工作区与暂存区的差异
        args << "diff" << filePath;
    }

    process.start("git", args);
    if (process.waitForFinished(5000)) {
        const QString output = QString::fromUtf8(process.readAllStandardOutput());
        if (!output.isEmpty()) {
            m_diffPreviewWidget->setPlainText(output);
        } else {
            m_diffPreviewWidget->setPlainText(tr("No differences found for file: %1").arg(filePath));
        }
    } else {
        m_diffPreviewWidget->setPlainText(tr("Failed to get diff for file: %1\nError: %2").arg(filePath, process.errorString()));
        qWarning() << "[GitStatusDialog] Failed to get diff for:" << filePath << process.errorString();
    }
}

void GitStatusDialog::updateButtonStates()
{
    const bool hasWorkingFiles = m_workingTreeWidget->selectedItems().count() > 0;
    const bool hasStagingFiles = m_stagingAreaWidget->selectedItems().count() > 0;
    const bool hasStagingArea = m_stagingAreaWidget->topLevelItemCount() > 0;

    m_addSelectedBtn->setEnabled(hasWorkingFiles);
    m_stageSelectedBtn->setEnabled(hasWorkingFiles);
    m_unstageSelectedBtn->setEnabled(hasStagingFiles);
    m_resetSelectedBtn->setEnabled(hasWorkingFiles || hasStagingFiles);
    m_commitBtn->setEnabled(hasStagingArea);
}

QList<QTreeWidgetItem *> GitStatusDialog::getSelectedFiles() const
{
    QList<QTreeWidgetItem *> selectedFiles;
    selectedFiles.append(m_workingTreeWidget->selectedItems());
    selectedFiles.append(m_stagingAreaWidget->selectedItems());
    return selectedFiles;
}

bool GitStatusDialog::hasSelectedFiles() const
{
    return !getSelectedFiles().isEmpty();
}

void GitStatusDialog::showFileContextMenu(const QPoint &pos)
{
    QTreeWidget *widget = qobject_cast<QTreeWidget *>(sender());
    if (!widget) return;

    QTreeWidgetItem *item = widget->itemAt(pos);
    if (!item) return;

    const QString status = item->data(0, Qt::UserRole).toString();
    const bool isWorkingFile = (widget == m_workingTreeWidget);
    const bool isStagingFile = (widget == m_stagingAreaWidget);

    // 根据文件状态和位置动态调整菜单项
    m_addAction->setEnabled(isWorkingFile && status == "??");
    m_stageAction->setEnabled(isWorkingFile && status != "??");
    m_unstageAction->setEnabled(isStagingFile);
    m_revertAction->setEnabled(true);
    m_removeAction->setEnabled(status != "??");
    m_diffAction->setEnabled(status != "??");

    m_contextMenu->exec(widget->mapToGlobal(pos));
}

void GitStatusDialog::addSelectedFiles()
{
    auto selectedItems = m_workingTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select files to add."));
        return;
    }

    QStringList filePaths;
    for (auto *item : selectedItems) {
        const QString status = item->data(0, Qt::UserRole).toString();
        if (status == "??") {   // 只添加未跟踪的文件
            filePaths << item->text(0);
        }
    }

    if (filePaths.isEmpty()) {
        QMessageBox::information(this, tr("No Untracked Files"), tr("Selected files are already tracked by Git."));
        return;
    }

    auto result = GitOperationUtils::addFiles(m_repositoryPath, filePaths);

    if (result.success) {
        // QMessageBox::information(this, tr("Success"), tr("Files added to Git successfully."));
        onRefreshClicked();
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to add files: %1").arg(result.error));
    }

    qDebug() << "[GitStatusDialog] Add operation completed for" << filePaths.size() << "files, success:" << result.success;
}

void GitStatusDialog::stageSelectedFiles()
{
    auto selectedItems = m_workingTreeWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select files to stage."));
        return;
    }

    QStringList filePaths;
    for (auto *item : selectedItems) {
        filePaths << item->text(0);
    }

    auto result = GitOperationUtils::stageFiles(m_repositoryPath, filePaths);

    if (result.success) {
        // QMessageBox::information(this, tr("Success"), tr("Files staged successfully."));
        onRefreshClicked();
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to stage files: %1").arg(result.error));
    }

    qDebug() << "[GitStatusDialog] Stage operation completed for" << filePaths.size() << "files, success:" << result.success;
}

void GitStatusDialog::unstageSelectedFiles()
{
    auto selectedItems = m_stagingAreaWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select files to unstage."));
        return;
    }

    QStringList filePaths;
    for (auto *item : selectedItems) {
        filePaths << item->text(0);
    }

    auto result = GitOperationUtils::unstageFiles(m_repositoryPath, filePaths);

    if (result.success) {
        // QMessageBox::information(this, tr("Success"), tr("Files unstaged successfully."));
        onRefreshClicked();
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to unstage files: %1").arg(result.error));
    }

    qDebug() << "[GitStatusDialog] Unstage operation completed for" << filePaths.size() << "files, success:" << result.success;
}

void GitStatusDialog::resetSelectedFiles()
{
    auto selectedFiles = getSelectedFiles();
    if (selectedFiles.isEmpty()) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select files to reset."));
        return;
    }

    const int result = QMessageBox::warning(this, tr("Confirm Reset"),
                                            tr("Are you sure you want to discard changes in %1 selected file(s)?\n\nThis action cannot be undone.").arg(selectedFiles.size()),
                                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    QStringList filePaths;
    for (auto *item : selectedFiles) {
        filePaths << item->text(0);
    }

    auto operationResult = GitOperationUtils::resetFiles(m_repositoryPath, filePaths);

    if (operationResult.success) {
        // QMessageBox::information(this, tr("Success"), tr("Files reset successfully."));
        onRefreshClicked();
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to reset files: %1").arg(operationResult.error));
    }

    qDebug() << "[GitStatusDialog] Reset operation completed for" << filePaths.size() << "files, success:" << operationResult.success;
}

void GitStatusDialog::commitSelectedFiles()
{
    qInfo() << "INFO: [GitStatusDialog::commitSelectedFiles] Opening commit dialog for repository:" << m_repositoryPath;

    // 使用带回调的commit对话框
    GitDialogManager::instance()->showCommitDialog(m_repositoryPath, this, [this](bool success) {
        if (success) {
            qInfo() << "INFO: [GitStatusDialog] Commit completed successfully, closing status dialog";
            accept();   // 关闭status窗口
        } else {
            qDebug() << "[GitStatusDialog] Commit cancelled or failed, refreshing status";
            onRefreshClicked();   // 刷新状态以反映可能的更改
        }
    });
}

void GitStatusDialog::onRefreshClicked()
{
    qDebug() << "[GitStatusDialog] Refreshing repository status";
    loadRepositoryStatus();
}

void GitStatusDialog::onFileDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (!item) {
        return;
    }

    Q_UNUSED(column)

    const QString filePath = item->text(0);
    const QString status = item->data(0, Qt::UserRole).toString();

    qInfo() << "INFO: [GitStatusDialog::onFileDoubleClicked] File:" << filePath << "Status:" << status;

    // 根据文件状态提供不同的双击行为
    if (status == "??") {
        // 未跟踪文件 - 打开文件查看内容
        QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePath);
        GitDialogManager::instance()->openFile(absolutePath, this);

    } else {
        // 已跟踪文件 - 显示差异
        GitDialogManager::instance()->showDiffDialog(m_repositoryPath, filePath, this);
    }
}

QString GitStatusDialog::getStatusDescription(const QString &status) const
{
    if (status.length() != 2) return tr("Unknown");

    const QChar index = status.at(0);
    const QChar workTree = status.at(1);

    QString desc;

    // 索引状态
    switch (index.toLatin1()) {
    case 'A':
        desc += tr("Added");
        break;
    case 'M':
        desc += tr("Modified");
        break;
    case 'D':
        desc += tr("Deleted");
        break;
    case 'R':
        desc += tr("Renamed");
        break;
    case 'C':
        desc += tr("Copied");
        break;
    case ' ':
        break;
    default:
        desc += tr("Unknown");
        break;
    }

    // 工作树状态
    if (workTree != ' ') {
        if (!desc.isEmpty()) desc += ", ";
        switch (workTree.toLatin1()) {
        case 'M':
            desc += tr("Modified in working tree");
            break;
        case 'D':
            desc += tr("Deleted in working tree");
            break;
        case '?':
            desc += tr("Untracked");
            break;
        default:
            desc += tr("Unknown working tree status");
            break;
        }
    }

    return desc.isEmpty() ? tr("Unchanged") : desc;
}

QIcon GitStatusDialog::getStatusIcon(const QString &status) const
{
    if (status.length() != 2) return QIcon();

    const QChar index = status.at(0);
    const QChar workTree = status.at(1);

    // 优先显示索引状态的图标
    switch (index.toLatin1()) {
    case 'A':
        return QIcon::fromTheme("list-add");
    case 'M':
        return QIcon::fromTheme("document-edit");
    case 'D':
        return QIcon::fromTheme("list-remove");
    case 'R':
        return QIcon::fromTheme("edit-rename");
    case 'C':
        return QIcon::fromTheme("edit-copy");
    default:
        // 如果索引没有状态，检查工作树状态
        switch (workTree.toLatin1()) {
        case 'M':
            return QIcon::fromTheme("document-edit");
        case 'D':
            return QIcon::fromTheme("list-remove");
        case '?':
            return QIcon::fromTheme("document-new");
        default:
            return QIcon::fromTheme("text-plain");
        }
    }
}

void GitStatusDialog::keyPressEvent(QKeyEvent *event)
{
    // 空格键预览文件
    if (event->key() == Qt::Key_Space) {
        QString filePath = getCurrentSelectedFilePath();
        if (!filePath.isEmpty()) {
            if (m_currentPreviewDialog) {
                // 如果已经有预览对话框打开，关闭它
                m_currentPreviewDialog->close();
                m_currentPreviewDialog = nullptr;
            } else {
                // 打开新的预览对话框
                previewSelectedFile();
            }
        }
        event->accept();   // 标记事件已处理
        return;
    }

    QDialog::keyPressEvent(event);
}

bool GitStatusDialog::eventFilter(QObject *watched, QEvent *event)
{
    // 捕获TreeWidget的键盘事件
    if ((watched == m_workingTreeWidget || watched == m_stagingAreaWidget) && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        // 空格键预览文件
        if (keyEvent->key() == Qt::Key_Space) {
            QString filePath = getCurrentSelectedFilePath();
            if (!filePath.isEmpty()) {
                if (m_currentPreviewDialog) {
                    // 如果已经有预览对话框打开，关闭它
                    m_currentPreviewDialog->close();
                    m_currentPreviewDialog = nullptr;
                } else {
                    // 打开新的预览对话框
                    previewSelectedFile();
                }
                return true;   // 事件已处理，不再传播
            }
        }
    }

    return QDialog::eventFilter(watched, event);
}

QString GitStatusDialog::getCurrentSelectedFilePath() const
{
    auto selectedFiles = getSelectedFiles();
    if (selectedFiles.isEmpty()) {
        return QString();
    }

    return selectedFiles.first()->text(0);
}

void GitStatusDialog::previewSelectedFile()
{
    QString filePath = getCurrentSelectedFilePath();
    if (filePath.isEmpty()) {
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
    m_currentPreviewDialog = GitDialogManager::instance()->showFilePreview(m_repositoryPath, filePath, this);

    // 连接对话框关闭信号，以便清理引用
    connect(m_currentPreviewDialog, &QDialog::finished, this, [this]() {
        m_currentPreviewDialog = nullptr;
    });

    qInfo() << "INFO: [GitStatusDialog] Opened file preview for:" << filePath;
}

#include "gitstatusdialog.moc"
