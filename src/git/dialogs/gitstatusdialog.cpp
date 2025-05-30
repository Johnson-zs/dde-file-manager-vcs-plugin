#include "gitstatusdialog.h"
#include "gitdiffdialog.h"
#include "gitcommitdialog.h"
#include "utils.h"
#include "gitcommandexecutor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QProgressDialog>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

/**
 * @brief 简单的Git差异语法高亮器
 */
class GitDiffHighlighter : public QSyntaxHighlighter
{
public:
    explicit GitDiffHighlighter(QTextDocument *parent = nullptr)
        : QSyntaxHighlighter(parent)
    {
        // 设置格式
        m_addedFormat.setForeground(QColor(46, 160, 67));   // 绿色 - 新增行
        m_addedFormat.setBackground(QColor(230, 255, 237));

        m_removedFormat.setForeground(QColor(203, 36, 49));   // 红色 - 删除行
        m_removedFormat.setBackground(QColor(255, 235, 233));

        m_headerFormat.setForeground(QColor(101, 109, 118));   // 灰色 - 头部信息
        m_headerFormat.setFontWeight(QFont::Bold);

        m_lineNumberFormat.setForeground(QColor(88, 166, 255));   // 蓝色 - 行号
        m_lineNumberFormat.setFontWeight(QFont::Bold);
    }

protected:
    void highlightBlock(const QString &text) override
    {
        // 新增行（以+开头）
        if (text.startsWith('+') && !text.startsWith("+++")) {
            setFormat(0, text.length(), m_addedFormat);
            return;
        }

        // 删除行（以-开头）
        if (text.startsWith('-') && !text.startsWith("---")) {
            setFormat(0, text.length(), m_removedFormat);
            return;
        }

        // 差异头部信息
        if (text.startsWith("diff --git") || text.startsWith("index ") || text.startsWith("+++") || text.startsWith("---")) {
            setFormat(0, text.length(), m_headerFormat);
            return;
        }

        // 行号信息（@@...@@）
        if (text.startsWith("@@") && text.contains("@@")) {
            setFormat(0, text.length(), m_lineNumberFormat);
            return;
        }
    }

private:
    QTextCharFormat m_addedFormat;
    QTextCharFormat m_removedFormat;
    QTextCharFormat m_headerFormat;
    QTextCharFormat m_lineNumberFormat;
};

GitStatusDialog::GitStatusDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_branchLabel(nullptr), m_statusSummary(nullptr), m_mainSplitter(nullptr), m_listSplitter(nullptr), m_workingTreeWidget(nullptr), m_stagingAreaWidget(nullptr), m_diffPreviewWidget(nullptr), m_refreshButton(nullptr), m_stageSelectedBtn(nullptr), m_unstageSelectedBtn(nullptr), m_addSelectedBtn(nullptr), m_resetSelectedBtn(nullptr), m_commitBtn(nullptr), m_contextMenu(nullptr)
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

    qDebug() << "[GitStatusDialog] Initialized with enhanced features for repository:" << repositoryPath;
}

void GitStatusDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // === 仓库信息区域 ===
    auto *infoGroup = new QGroupBox(tr("Repository Information"), this);
    infoGroup->setMaximumHeight(80);  // 限制最大高度，保持紧凑
    auto *infoLayout = new QHBoxLayout(infoGroup);  // 使用水平布局更紧凑
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
    infoLayout->addStretch();  // 添加弹性空间

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

    m_diffPreviewWidget = new QTextEdit(this);
    m_diffPreviewWidget->setReadOnly(true);
    m_diffPreviewWidget->setFont(QFont("Courier", 9));
    m_diffPreviewWidget->setPlaceholderText(tr("Select a file to view its differences here..."));

    // 应用语法高亮
    auto *highlighter = new GitDiffHighlighter(m_diffPreviewWidget->document());
    Q_UNUSED(highlighter)

    previewLayout->addWidget(m_diffPreviewWidget);
    m_mainSplitter->addWidget(previewGroup);

    // 设置主分割器比例和属性
    m_mainSplitter->setSizes({ 800, 600 });  // 调整初始比例，给文件列表更多空间
    m_mainSplitter->setStretchFactor(0, 1);  // 文件列表区域可拉伸
    m_mainSplitter->setStretchFactor(1, 1);  // 预览区域也可拉伸
    m_mainSplitter->setChildrenCollapsible(false);  // 防止子窗口被完全折叠
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

    m_addAction = m_contextMenu->addAction(QIcon::fromTheme("list-add"), tr("Add to Git"));
    m_stageAction = m_contextMenu->addAction(QIcon::fromTheme("go-up"), tr("Stage"));
    m_unstageAction = m_contextMenu->addAction(QIcon::fromTheme("go-down"), tr("Unstage"));
    m_contextMenu->addSeparator();
    m_revertAction = m_contextMenu->addAction(QIcon::fromTheme("edit-undo"), tr("Revert Changes"));
    m_removeAction = m_contextMenu->addAction(QIcon::fromTheme("list-remove"), tr("Remove from Git"));
    m_contextMenu->addSeparator();
    m_diffAction = m_contextMenu->addAction(QIcon::fromTheme("document-properties"), tr("View Diff..."));

    // 连接菜单动作
    connect(m_addAction, &QAction::triggered, this, &GitStatusDialog::addSelectedFiles);
    connect(m_stageAction, &QAction::triggered, this, &GitStatusDialog::stageSelectedFiles);
    connect(m_unstageAction, &QAction::triggered, this, &GitStatusDialog::unstageSelectedFiles);
    connect(m_revertAction, &QAction::triggered, this, &GitStatusDialog::resetSelectedFiles);
    connect(m_diffAction, &QAction::triggered, [this]() {
        auto selectedFiles = getSelectedFiles();
        if (!selectedFiles.isEmpty()) {
            const QString filePath = selectedFiles.first()->text(0);
            auto *diffDialog = new GitDiffDialog(m_repositoryPath, filePath, this);
            diffDialog->show();
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

    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    // === 获取当前分支信息 ===
    process.start("git", QStringList() << "branch"
                                       << "--show-current");
    if (process.waitForFinished(3000)) {
        QString branch = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (branch.isEmpty()) {
            branch = tr("(detached HEAD)");
        }
        m_branchLabel->setText(tr("Current Branch: %1").arg(branch));
    } else {
        m_branchLabel->setText(tr("Current Branch: Unknown"));
        qWarning() << "[GitStatusDialog] Failed to get current branch:" << process.errorString();
    }

    // === 获取文件状态 ===
    process.start("git", QStringList() << "status"
                                       << "--porcelain");
    if (!process.waitForFinished(5000)) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to get repository status: %1").arg(process.errorString()));
        return;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        if (line.length() < 3) continue;

        const QString status = line.left(2);
        const QString filePath = line.mid(3);

        if (status.at(0) != ' ' && status.at(0) != '?') {
            // 已暂存的文件
            auto *item = new QTreeWidgetItem(m_stagingAreaWidget);
            item->setText(0, filePath);
            item->setText(1, getStatusDescription(status));
            item->setIcon(0, getStatusIcon(status));
            item->setData(0, Qt::UserRole, status);   // 存储状态信息
            item->setToolTip(0, tr("Status: %1\nRight-click for options").arg(getStatusDescription(status)));
        }

        if (status.at(1) != ' ' || status == "??") {
            // 工作区文件（已修改但未暂存或未跟踪）
            auto *item = new QTreeWidgetItem(m_workingTreeWidget);
            item->setText(0, filePath);
            item->setText(1, getStatusDescription(status));
            item->setIcon(0, getStatusIcon(status));
            item->setData(0, Qt::UserRole, status);
            item->setToolTip(0, tr("Status: %1\nRight-click for options").arg(getStatusDescription(status)));
        }
    }

    updateStatusSummary();
    updateButtonStates();

    qDebug() << "[GitStatusDialog] Repository status loaded successfully";
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

    bool success = true;
    QString errorMessage;

    auto *progressDialog = new QProgressDialog(tr("Adding files to Git..."), tr("Cancel"), 0, filePaths.size(), this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->show();

    for (int i = 0; i < filePaths.size(); ++i) {
        if (progressDialog->wasCanceled()) {
            break;
        }

        const QString &filePath = filePaths.at(i);
        progressDialog->setLabelText(tr("Adding: %1").arg(filePath));
        progressDialog->setValue(i);

        // 使用QProcess直接执行git add命令
        QProcess process;
        process.setWorkingDirectory(m_repositoryPath);
        process.start("git", QStringList() << "add" << filePath);
        
        if (!process.waitForFinished(5000) || process.exitCode() != 0) {
            success = false;
            errorMessage = tr("Failed to add file: %1\nError: %2").arg(filePath, QString::fromUtf8(process.readAllStandardError()));
            break;
        }

        QApplication::processEvents();
    }

    progressDialog->deleteLater();

    if (success) {
        QMessageBox::information(this, tr("Success"), tr("Files added to Git successfully."));
        onRefreshClicked();
    } else {
        QMessageBox::critical(this, tr("Error"), errorMessage);
    }

    qDebug() << "[GitStatusDialog] Add operation completed for" << filePaths.size() << "files, success:" << success;
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

    bool success = true;
    QString errorMessage;

    auto *progressDialog = new QProgressDialog(tr("Staging files..."), tr("Cancel"), 0, filePaths.size(), this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->show();

    for (int i = 0; i < filePaths.size(); ++i) {
        if (progressDialog->wasCanceled()) {
            break;
        }

        const QString &filePath = filePaths.at(i);
        progressDialog->setLabelText(tr("Staging: %1").arg(filePath));
        progressDialog->setValue(i);

        // 使用QProcess直接执行git add命令
        QProcess process;
        process.setWorkingDirectory(m_repositoryPath);
        process.start("git", QStringList() << "add" << filePath);
        
        if (!process.waitForFinished(5000) || process.exitCode() != 0) {
            success = false;
            errorMessage = tr("Failed to stage file: %1\nError: %2").arg(filePath, QString::fromUtf8(process.readAllStandardError()));
            break;
        }

        QApplication::processEvents();
    }

    progressDialog->deleteLater();

    if (success) {
        QMessageBox::information(this, tr("Success"), tr("Files staged successfully."));
        onRefreshClicked();
    } else {
        QMessageBox::critical(this, tr("Error"), errorMessage);
    }

    qDebug() << "[GitStatusDialog] Stage operation completed for" << filePaths.size() << "files, success:" << success;
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

    bool success = true;
    QString errorMessage;

    auto *progressDialog = new QProgressDialog(tr("Unstaging files..."), tr("Cancel"), 0, filePaths.size(), this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->show();

    for (int i = 0; i < filePaths.size(); ++i) {
        if (progressDialog->wasCanceled()) {
            break;
        }

        const QString &filePath = filePaths.at(i);
        progressDialog->setLabelText(tr("Unstaging: %1").arg(filePath));
        progressDialog->setValue(i);

        // 使用QProcess直接执行git reset HEAD命令
        QProcess process;
        process.setWorkingDirectory(m_repositoryPath);
        process.start("git", QStringList() << "reset" << "HEAD" << filePath);
        
        if (!process.waitForFinished(5000) || process.exitCode() != 0) {
            success = false;
            errorMessage = tr("Failed to unstage file: %1\nError: %2").arg(filePath, QString::fromUtf8(process.readAllStandardError()));
            break;
        }

        QApplication::processEvents();
    }

    progressDialog->deleteLater();

    if (success) {
        QMessageBox::information(this, tr("Success"), tr("Files unstaged successfully."));
        onRefreshClicked();
    } else {
        QMessageBox::critical(this, tr("Error"), errorMessage);
    }

    qDebug() << "[GitStatusDialog] Unstage operation completed for" << filePaths.size() << "files, success:" << success;
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

    bool success = true;
    QString errorMessage;

    auto *progressDialog = new QProgressDialog(tr("Resetting files..."), tr("Cancel"), 0, filePaths.size(), this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->show();

    for (int i = 0; i < filePaths.size(); ++i) {
        if (progressDialog->wasCanceled()) {
            break;
        }

        const QString &filePath = filePaths.at(i);
        progressDialog->setLabelText(tr("Resetting: %1").arg(filePath));
        progressDialog->setValue(i);

        // 使用QProcess直接执行git checkout HEAD命令
        QProcess process;
        process.setWorkingDirectory(m_repositoryPath);
        process.start("git", QStringList() << "checkout" << "HEAD" << "--" << filePath);
        
        if (!process.waitForFinished(5000) || process.exitCode() != 0) {
            success = false;
            errorMessage = tr("Failed to reset file: %1\nError: %2").arg(filePath, QString::fromUtf8(process.readAllStandardError()));
            break;
        }

        QApplication::processEvents();
    }

    progressDialog->deleteLater();

    if (success) {
        QMessageBox::information(this, tr("Success"), tr("Files reset successfully."));
        onRefreshClicked();
    } else {
        QMessageBox::critical(this, tr("Error"), errorMessage);
    }

    qDebug() << "[GitStatusDialog] Reset operation completed for" << filePaths.size() << "files, success:" << success;
}

void GitStatusDialog::commitSelectedFiles()
{
    if (m_stagingAreaWidget->topLevelItemCount() == 0) {
        QMessageBox::information(this, tr("No Staged Files"), tr("There are no files in the staging area to commit."));
        return;
    }

    auto *commitDialog = new GitCommitDialog(m_repositoryPath, this);
    commitDialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(commitDialog, &QDialog::accepted, [this]() {
        onRefreshClicked();   // 刷新状态
        QMessageBox::information(this, tr("Success"), tr("Commit completed successfully."));
    });

    commitDialog->show();
}

void GitStatusDialog::onRefreshClicked()
{
    qDebug() << "[GitStatusDialog] Refreshing repository status";
    loadRepositoryStatus();
}

void GitStatusDialog::onFileDoubleClicked(QListWidgetItem *item)
{
    // 保留原有的双击功能用于向后兼容
    if (!item) return;

    const QString filePath = item->text();
    const QString fullPath = QDir(m_repositoryPath).absoluteFilePath(filePath);
    const QString status = item->data(Qt::UserRole).toString();

    qDebug() << "[GitStatusDialog] File double-clicked:" << filePath << "Status:" << status;

    if (status == "??") {
        // 未跟踪文件：直接打开文件
        if (QFileInfo::exists(fullPath)) {
            QProcess::startDetached("xdg-open", QStringList() << fullPath);
        } else {
            QMessageBox::information(this, tr("File Not Found"),
                                     tr("The file '%1' does not exist in the working directory.").arg(filePath));
        }
    } else {
        // 已修改或已暂存文件：显示diff界面
        auto *diffDialog = new GitDiffDialog(m_repositoryPath, filePath, this);
        diffDialog->setAttribute(Qt::WA_DeleteOnClose);
        diffDialog->show();
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
