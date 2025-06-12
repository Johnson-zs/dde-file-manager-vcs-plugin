#include "gitcleandialog.h"
#include "../gitoperationservice.h"
#include "../utils.h"

#include <QApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QHeaderView>
#include <QSizePolicy>
#include <QFont>

GitCleanDialog::GitCleanDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent)
    , m_repositoryPath(repositoryPath)
    , m_operationService(new GitOperationService(this))
    , m_isOperationInProgress(false)
{
    setWindowTitle(tr("Git Clean - %1").arg(QFileInfo(repositoryPath).fileName()));
    setWindowIcon(QIcon::fromTheme("edit-delete"));
    resize(800, 600);
    
    setupUI();
    
    // 连接操作服务信号
    connect(m_operationService, &GitOperationService::operationCompleted,
            this, &GitCleanDialog::onOperationCompleted);
    
    // 初始化界面状态
    updateButtonStates();
    onOptionsChanged(); // 更新选项描述
    
    qInfo() << "INFO: [GitCleanDialog] Initialized for repository:" << repositoryPath;
}

void GitCleanDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);
    
    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(m_mainSplitter);
    
    // 设置各个区域
    setupOptionsArea();
    setupPreviewArea();
    setupButtonArea();
    
    // 添加按钮区域到主布局
    mainLayout->addWidget(m_buttonGroup);
    
    // 设置分割器比例
    m_mainSplitter->setSizes({300, 500});
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
}

void GitCleanDialog::setupOptionsArea()
{
    m_optionsGroup = new QGroupBox(tr("Clean Options"), this);
    auto *optionsLayout = new QVBoxLayout(m_optionsGroup);
    optionsLayout->setSpacing(8);
    
    // 警告标签
    m_warningLabel = new QLabel(this);
    m_warningLabel->setText(tr("⚠️ Warning: Git clean will permanently delete files!"));
    m_warningLabel->setStyleSheet("QLabel { color: #d32f2f; font-weight: bold; background-color: #ffebee; padding: 8px; border-radius: 4px; }");
    m_warningLabel->setWordWrap(true);
    optionsLayout->addWidget(m_warningLabel);
    
    // 选项复选框
    m_forceCheckBox = new QCheckBox(tr("Force clean (-f)"), this);
    m_forceCheckBox->setToolTip(tr("Force removal of untracked files. Required for actual deletion."));
    m_forceCheckBox->setChecked(false);
    optionsLayout->addWidget(m_forceCheckBox);
    
    m_directoriesCheckBox = new QCheckBox(tr("Remove directories (-d)"), this);
    m_directoriesCheckBox->setToolTip(tr("Recursively remove untracked directories in addition to untracked files."));
    m_directoriesCheckBox->setChecked(false);
    optionsLayout->addWidget(m_directoriesCheckBox);
    
    m_ignoredCheckBox = new QCheckBox(tr("Remove ignored files (-x)"), this);
    m_ignoredCheckBox->setToolTip(tr("Remove files ignored by .gitignore in addition to untracked files."));
    m_ignoredCheckBox->setChecked(false);
    optionsLayout->addWidget(m_ignoredCheckBox);
    
    m_onlyIgnoredCheckBox = new QCheckBox(tr("Remove only ignored files (-X)"), this);
    m_onlyIgnoredCheckBox->setToolTip(tr("Remove only files ignored by .gitignore, keep untracked files."));
    m_onlyIgnoredCheckBox->setChecked(false);
    optionsLayout->addWidget(m_onlyIgnoredCheckBox);
    
    // 选项描述标签
    m_optionsDescLabel = new QLabel(this);
    m_optionsDescLabel->setWordWrap(true);
    m_optionsDescLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; background-color: #f5f5f5; padding: 6px; border-radius: 3px; }");
    optionsLayout->addWidget(m_optionsDescLabel);
    
    optionsLayout->addStretch();
    
    // 连接信号
    connect(m_forceCheckBox, &QCheckBox::toggled, this, &GitCleanDialog::onOptionsChanged);
    connect(m_directoriesCheckBox, &QCheckBox::toggled, this, &GitCleanDialog::onOptionsChanged);
    connect(m_ignoredCheckBox, &QCheckBox::toggled, this, &GitCleanDialog::onOptionsChanged);
    connect(m_onlyIgnoredCheckBox, &QCheckBox::toggled, this, &GitCleanDialog::onOptionsChanged);
    
    // 互斥选项处理
    connect(m_ignoredCheckBox, &QCheckBox::toggled, [this](bool checked) {
        if (checked) {
            m_onlyIgnoredCheckBox->setChecked(false);
        }
    });
    connect(m_onlyIgnoredCheckBox, &QCheckBox::toggled, [this](bool checked) {
        if (checked) {
            m_ignoredCheckBox->setChecked(false);
        }
    });
    
    m_mainSplitter->addWidget(m_optionsGroup);
}

void GitCleanDialog::setupPreviewArea()
{
    m_previewGroup = new QGroupBox(tr("Files to be removed"), this);
    auto *previewLayout = new QVBoxLayout(m_previewGroup);
    previewLayout->setSpacing(8);
    
    // 预览标题和文件数量
    auto *titleLayout = new QHBoxLayout();
    m_previewTitleLabel = new QLabel(tr("Preview (dry run):"), this);
    m_previewTitleLabel->setStyleSheet("QLabel { font-weight: bold; }");
    titleLayout->addWidget(m_previewTitleLabel);
    
    m_fileCountLabel = new QLabel(tr("No files selected"), this);
    m_fileCountLabel->setStyleSheet("QLabel { color: #666; }");
    titleLayout->addStretch();
    titleLayout->addWidget(m_fileCountLabel);
    previewLayout->addLayout(titleLayout);
    
    // 文件列表
    m_fileListWidget = new QListWidget(this);
    m_fileListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileListWidget->setAlternatingRowColors(true);
    m_fileListWidget->setToolTip(tr("Files and directories that will be removed by git clean"));
    previewLayout->addWidget(m_fileListWidget);
    
    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    previewLayout->addWidget(m_progressBar);
    
    // 连接信号
    connect(m_fileListWidget, &QListWidget::itemSelectionChanged,
            this, &GitCleanDialog::onFileSelectionChanged);
    
    m_mainSplitter->addWidget(m_previewGroup);
}

void GitCleanDialog::setupButtonArea()
{
    m_buttonGroup = new QGroupBox(this);
    m_buttonGroup->setFlat(true);
    auto *buttonLayout = new QHBoxLayout(m_buttonGroup);
    buttonLayout->setContentsMargins(0, 8, 0, 0);
    
    // 预览按钮
    m_previewButton = new QPushButton(tr("Preview"), this);
    m_previewButton->setIcon(QIcon::fromTheme("view-preview"));
    m_previewButton->setToolTip(tr("Show which files would be removed (git clean --dry-run)"));
    buttonLayout->addWidget(m_previewButton);
    
    // 刷新按钮
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setIcon(QIcon::fromTheme("view-refresh"));
    m_refreshButton->setToolTip(tr("Refresh the file list"));
    buttonLayout->addWidget(m_refreshButton);
    
    buttonLayout->addStretch();
    
    // 执行清理按钮
    m_cleanButton = new QPushButton(tr("Clean Repository"), this);
    m_cleanButton->setIcon(QIcon::fromTheme("edit-delete"));
    m_cleanButton->setStyleSheet("QPushButton { background-color: #d32f2f; color: white; font-weight: bold; padding: 8px 16px; }");
    m_cleanButton->setToolTip(tr("Execute git clean with selected options"));
    buttonLayout->addWidget(m_cleanButton);
    
    // 取消按钮
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setIcon(QIcon::fromTheme("dialog-cancel"));
    buttonLayout->addWidget(m_cancelButton);
    
    // 连接信号
    connect(m_previewButton, &QPushButton::clicked, this, &GitCleanDialog::onPreviewClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &GitCleanDialog::onRefreshClicked);
    connect(m_cleanButton, &QPushButton::clicked, this, &GitCleanDialog::onCleanClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void GitCleanDialog::onPreviewClicked()
{
    qInfo() << "INFO: [GitCleanDialog::onPreviewClicked] Loading file preview";
    loadFilePreview();
}

void GitCleanDialog::onRefreshClicked()
{
    qInfo() << "INFO: [GitCleanDialog::onRefreshClicked] Refreshing file list";
    loadFilePreview();
}

void GitCleanDialog::onCleanClicked()
{
    if (m_isOperationInProgress) {
        return;
    }
    
    // 安全检查
    if (!m_forceCheckBox->isChecked()) {
        QMessageBox::warning(this, tr("Force Required"),
                           tr("You must check 'Force clean' option to perform the actual clean operation.\n\n"
                              "This is a safety measure to prevent accidental file deletion."));
        return;
    }
    
    if (m_previewFiles.isEmpty()) {
        QMessageBox::information(this, tr("No Files to Clean"),
                                tr("No files found to clean with the current options."));
        return;
    }
    
    // 确认对话框
    QString message = tr("Are you sure you want to permanently delete %1 file(s)?\n\n"
                        "This action cannot be undone!\n\n"
                        "Options: %2")
                        .arg(m_previewFiles.size())
                        .arg(getOptionsDescription());
    
    int ret = QMessageBox::warning(this, tr("Confirm Git Clean"),
                                  message,
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        performCleanOperation();
    }
}

void GitCleanDialog::onOptionsChanged()
{
    updateButtonStates();
    
    // 更新选项描述
    QString desc = getOptionsDescription();
    if (desc.isEmpty()) {
        m_optionsDescLabel->setText(tr("Select options above to configure git clean behavior."));
    } else {
        m_optionsDescLabel->setText(tr("Command: git clean %1").arg(desc));
    }
    
    // 清空预览（选项改变后需要重新预览）
    clearPreview();
}

void GitCleanDialog::onOperationCompleted(const QString &operation, bool success, const QString &message)
{
    m_isOperationInProgress = false;
    m_progressBar->setVisible(false);
    updateButtonStates();
    
    qInfo() << "INFO: [GitCleanDialog::onOperationCompleted] Operation:" << operation 
            << "Success:" << success << "Message:" << message;
    
    if (operation.contains("Clean")) {
        if (success) {
            QMessageBox::information(this, tr("Clean Completed"),
                                   tr("Git clean operation completed successfully.\n\n%1").arg(message));
            // 刷新预览以显示更新后的状态
            loadFilePreview();
        } else {
            QMessageBox::critical(this, tr("Clean Failed"),
                                tr("Git clean operation failed:\n\n%1").arg(message));
        }
    }
}

void GitCleanDialog::onFileSelectionChanged()
{
    // 可以在这里添加选中文件的详细信息显示
    QStringList selectedFiles = getSelectedFiles();
    if (!selectedFiles.isEmpty()) {
        m_previewTitleLabel->setText(tr("Preview (%1 selected):").arg(selectedFiles.size()));
    } else {
        m_previewTitleLabel->setText(tr("Preview (dry run):"));
    }
}

void GitCleanDialog::loadFilePreview()
{
    if (m_isOperationInProgress) {
        return;
    }
    
    m_isOperationInProgress = true;
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // 不确定进度
    updateButtonStates();
    
    // 获取预览文件列表
    QStringList files = m_operationService->getCleanPreview(
        m_repositoryPath,
        m_directoriesCheckBox->isChecked(),
        m_ignoredCheckBox->isChecked(),
        m_onlyIgnoredCheckBox->isChecked()
    );
    
    m_previewFiles = files;
    
    // 更新UI
    m_fileListWidget->clear();
    
    if (files.isEmpty()) {
        auto *item = new QListWidgetItem(tr("No files to clean with current options"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(QBrush(QColor("#666")));
        m_fileListWidget->addItem(item);
        m_fileCountLabel->setText(tr("No files found"));
    } else {
        for (const QString &file : files) {
            auto *item = new QListWidgetItem(file);
            item->setIcon(QIcon::fromTheme("text-x-generic"));
            item->setToolTip(tr("File: %1").arg(file));
            m_fileListWidget->addItem(item);
        }
        m_fileCountLabel->setText(tr("%1 file(s) will be removed").arg(files.size()));
    }
    
    m_isOperationInProgress = false;
    m_progressBar->setVisible(false);
    updateButtonStates();
    
    qInfo() << "INFO: [GitCleanDialog::loadFilePreview] Found" << files.size() << "files to clean";
}

void GitCleanDialog::updateButtonStates()
{
    bool hasOptions = hasSelectedOptions();
    bool hasFiles = !m_previewFiles.isEmpty();
    bool canClean = hasOptions && hasFiles && m_forceCheckBox->isChecked() && !m_isOperationInProgress;
    
    m_previewButton->setEnabled(hasOptions && !m_isOperationInProgress);
    m_refreshButton->setEnabled(!m_isOperationInProgress);
    m_cleanButton->setEnabled(canClean);
    
    // 更新清理按钮的样式
    if (canClean) {
        m_cleanButton->setStyleSheet("QPushButton { background-color: #d32f2f; color: white; font-weight: bold; padding: 8px 16px; }");
    } else {
        m_cleanButton->setStyleSheet("QPushButton { background-color: #ccc; color: #666; font-weight: bold; padding: 8px 16px; }");
    }
}

void GitCleanDialog::clearPreview()
{
    m_fileListWidget->clear();
    m_previewFiles.clear();
    m_fileCountLabel->setText(tr("Click 'Preview' to see files"));
}

void GitCleanDialog::performCleanOperation()
{
    m_isOperationInProgress = true;
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    updateButtonStates();
    
    qInfo() << "INFO: [GitCleanDialog::performCleanOperation] Starting git clean operation";
    
    // 执行实际的clean操作
    m_operationService->cleanRepository(
        m_repositoryPath.toStdString(),
        m_forceCheckBox->isChecked(),
        m_directoriesCheckBox->isChecked(),
        m_ignoredCheckBox->isChecked(),
        m_onlyIgnoredCheckBox->isChecked(),
        false // 不是dry-run
    );
}

bool GitCleanDialog::hasSelectedOptions() const
{
    return m_forceCheckBox->isChecked() || 
           m_directoriesCheckBox->isChecked() || 
           m_ignoredCheckBox->isChecked() || 
           m_onlyIgnoredCheckBox->isChecked();
}

QString GitCleanDialog::getOptionsDescription() const
{
    QStringList options;
    
    if (m_forceCheckBox->isChecked()) {
        options << "-f";
    }
    if (m_directoriesCheckBox->isChecked()) {
        options << "-d";
    }
    if (m_onlyIgnoredCheckBox->isChecked()) {
        options << "-X";
    } else if (m_ignoredCheckBox->isChecked()) {
        options << "-x";
    }
    
    return options.join(" ");
}

int GitCleanDialog::getFileCount() const
{
    return m_previewFiles.size();
}

QStringList GitCleanDialog::getSelectedFiles() const
{
    QStringList selectedFiles;
    for (int i = 0; i < m_fileListWidget->count(); ++i) {
        QListWidgetItem *item = m_fileListWidget->item(i);
        if (item && item->isSelected()) {
            selectedFiles << item->text();
        }
    }
    return selectedFiles;
} 