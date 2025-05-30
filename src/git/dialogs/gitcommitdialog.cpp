#include "gitcommitdialog.h"
#include "gitoperationdialog.h"
#include "gitdialogs.h"
#include "../gitcommandexecutor.h"
#include "../gitstatusparser.h"
#include "../gitoperationutils.h"

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
#include <QMenu>
#include <QAction>
#include <QStandardItem>
#include <QTreeView>
#include <QProgressDialog>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QClipboard>
#include <QRegularExpression>

// ============================================================================
// GitFileItem Implementation
// ============================================================================

GitFileItem::GitFileItem(const QString &filePath, Status status, const QString &statusText)
    : m_filePath(filePath), m_status(status), m_statusText(statusText), m_checked(false)
{
    // Set default checked state based on status - modified files should be checked by default
    if (status == Status::Modified || status == Status::Deleted) {
        m_checked = true;
    } else if (status == Status::Staged || status == Status::StagedModified || status == Status::StagedAdded || status == Status::StagedDeleted) {
        m_checked = true;
    }
}

QString GitFileItem::fileName() const
{
    return QFileInfo(m_filePath).fileName();
}

bool GitFileItem::isStaged() const
{
    return m_status == Status::Staged || m_status == Status::StagedModified || m_status == Status::StagedAdded || m_status == Status::StagedDeleted || m_status == Status::Renamed || m_status == Status::Copied;
}

QIcon GitFileItem::statusIcon() const
{
    switch (m_status) {
    case Status::Modified:
    case Status::StagedModified:
        return QIcon::fromTheme("document-edit");
    case Status::Staged:
    case Status::StagedAdded:
        return QIcon::fromTheme("list-add");
    case Status::Deleted:
    case Status::StagedDeleted:
        return QIcon::fromTheme("list-remove");
    case Status::Untracked:
        return QIcon::fromTheme("document-new");
    case Status::Renamed:
        return QIcon::fromTheme("edit-rename");
    case Status::Copied:
        return QIcon::fromTheme("edit-copy");
    default:
        return QIcon::fromTheme("document-properties");
    }
}

QString GitFileItem::statusDisplayText() const
{
    if (!m_statusText.isEmpty()) {
        return m_statusText;
    }

    switch (m_status) {
    case Status::Modified:
        return QObject::tr("Modified");
    case Status::Staged:
        return QObject::tr("Staged");
    case Status::StagedModified:
        return QObject::tr("Staged (Modified)");
    case Status::StagedAdded:
        return QObject::tr("Staged (Added)");
    case Status::StagedDeleted:
        return QObject::tr("Staged (Deleted)");
    case Status::Deleted:
        return QObject::tr("Deleted");
    case Status::Untracked:
        return QObject::tr("Untracked");
    case Status::Renamed:
        return QObject::tr("Renamed");
    case Status::Copied:
        return QObject::tr("Copied");
    default:
        return QObject::tr("Unknown");
    }
}

// ============================================================================
// GitFileModel Implementation
// ============================================================================

GitFileModel::GitFileModel(QObject *parent)
    : QStandardItemModel(parent)
{
    setHorizontalHeaderLabels({ tr("File"), tr("Status"), tr("Path") });
}

void GitFileModel::setFiles(const QList<std::shared_ptr<GitFileItem>> &files)
{
    clear();
    setHorizontalHeaderLabels({ tr("File"), tr("Status"), tr("Path") });

    m_files = files;

    for (auto file : files) {
        addFile(file);
    }
}

void GitFileModel::addFile(std::shared_ptr<GitFileItem> file)
{
    auto *fileNameItem = new QStandardItem(file->fileName());
    auto *statusItem = new QStandardItem(file->statusDisplayText());
    auto *pathItem = new QStandardItem(file->displayPath());

    fileNameItem->setIcon(file->statusIcon());
    statusItem->setIcon(file->statusIcon());

    fileNameItem->setCheckable(true);
    fileNameItem->setCheckState(file->isChecked() ? Qt::Checked : Qt::Unchecked);

    // Store file data
    fileNameItem->setData(QVariant::fromValue(file), FileItemRole);
    fileNameItem->setData(file->filePath(), FilePathRole);
    fileNameItem->setData(static_cast<int>(file->status()), StatusRole);
    fileNameItem->setData(file->isChecked(), IsCheckedRole);

    statusItem->setData(QVariant::fromValue(file), FileItemRole);
    pathItem->setData(QVariant::fromValue(file), FileItemRole);

    appendRow({ fileNameItem, statusItem, pathItem });
}

void GitFileModel::updateFile(const QString &filePath, GitFileItem::Status status)
{
    auto item = findItemByPath(filePath);
    if (!item) {
        return;
    }

    auto file = item->data(FileItemRole).value<std::shared_ptr<GitFileItem>>();
    if (file) {
        // Update the file item
        file = std::make_shared<GitFileItem>(filePath, status);
        updateModelItem(item, file);

        // Update in our list
        for (auto &f : m_files) {
            if (f->filePath() == filePath) {
                f = file;
                break;
            }
        }
    }
}

void GitFileModel::removeFile(const QString &filePath)
{
    auto item = findItemByPath(filePath);
    if (item) {
        removeRow(item->row());

        // Remove from our list
        m_files.removeIf([&filePath](const std::shared_ptr<GitFileItem> &file) {
            return file->filePath() == filePath;
        });
    }
}

void GitFileModel::clear()
{
    QStandardItemModel::clear();
    m_files.clear();
}

std::shared_ptr<GitFileItem> GitFileModel::getFileItem(const QString &filePath) const
{
    for (const auto &file : m_files) {
        if (file->filePath() == filePath) {
            return file;
        }
    }
    return nullptr;
}

QList<std::shared_ptr<GitFileItem>> GitFileModel::getCheckedFiles() const
{
    QList<std::shared_ptr<GitFileItem>> checkedFiles;

    for (int row = 0; row < rowCount(); ++row) {
        auto item = this->item(row, 0);
        if (item && item->checkState() == Qt::Checked) {
            auto file = item->data(FileItemRole).value<std::shared_ptr<GitFileItem>>();
            if (file) {
                checkedFiles.append(file);
            }
        }
    }

    return checkedFiles;
}

QList<std::shared_ptr<GitFileItem>> GitFileModel::getAllFiles() const
{
    return m_files;
}

bool GitFileModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == Qt::CheckStateRole && index.column() == 0) {
        bool checked = value.toInt() == Qt::Checked;
        auto item = itemFromIndex(index);
        if (item) {
            auto file = item->data(FileItemRole).value<std::shared_ptr<GitFileItem>>();
            if (file) {
                file->setChecked(checked);
                item->setData(checked, IsCheckedRole);
                Q_EMIT fileCheckStateChanged(file->filePath(), checked);
            }
        }
    }

    return QStandardItemModel::setData(index, value, role);
}

Qt::ItemFlags GitFileModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QStandardItemModel::flags(index);

    if (index.column() == 0) {
        flags |= Qt::ItemIsUserCheckable;
    }

    return flags;
}

void GitFileModel::updateModelItem(QStandardItem *item, std::shared_ptr<GitFileItem> fileItem)
{
    if (!item || !fileItem) {
        return;
    }

    auto row = item->row();
    auto statusItem = this->item(row, 1);
    auto pathItem = this->item(row, 2);

    item->setText(fileItem->fileName());
    item->setIcon(fileItem->statusIcon());
    item->setData(QVariant::fromValue(fileItem), FileItemRole);
    item->setData(fileItem->filePath(), FilePathRole);
    item->setData(static_cast<int>(fileItem->status()), StatusRole);
    item->setData(fileItem->isChecked(), IsCheckedRole);

    if (statusItem) {
        statusItem->setText(fileItem->statusDisplayText());
        statusItem->setIcon(fileItem->statusIcon());
        statusItem->setData(QVariant::fromValue(fileItem), FileItemRole);
    }

    if (pathItem) {
        pathItem->setText(fileItem->displayPath());
        pathItem->setData(QVariant::fromValue(fileItem), FileItemRole);
    }
}

QStandardItem *GitFileModel::findItemByPath(const QString &filePath) const
{
    for (int row = 0; row < rowCount(); ++row) {
        auto item = this->item(row, 0);
        if (item && item->data(FilePathRole).toString() == filePath) {
            return item;
        }
    }
    return nullptr;
}

// ============================================================================
// GitFileProxyModel Implementation
// ============================================================================

GitFileProxyModel::GitFileProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent), m_filterType(AllFiles)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void GitFileProxyModel::setFilterType(FilterType type)
{
    m_filterType = type;
    invalidateFilter();
}

void GitFileProxyModel::setSearchText(const QString &text)
{
    m_searchText = text.trimmed();
    invalidateFilter();
}

bool GitFileProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    auto model = qobject_cast<const GitFileModel *>(sourceModel());
    if (!model) {
        return true;
    }

    auto index = model->index(sourceRow, 0, sourceParent);
    auto file = index.data(GitFileModel::FileItemRole).value<std::shared_ptr<GitFileItem>>();

    if (!file) {
        return false;
    }

    // Apply filter type
    bool passesTypeFilter = true;
    switch (m_filterType) {
    case StagedFiles:
        passesTypeFilter = file->isStaged();
        break;
    case ModifiedFiles:
        passesTypeFilter = !file->isStaged() && file->status() != GitFileItem::Status::Untracked;
        break;
    case UntrackedFiles:
        passesTypeFilter = file->status() == GitFileItem::Status::Untracked;
        break;
    case AllFiles:
    default:
        passesTypeFilter = true;
        break;
    }

    // Apply search filter
    bool passesSearchFilter = true;
    if (!m_searchText.isEmpty()) {
        passesSearchFilter = file->fileName().contains(m_searchText, Qt::CaseInsensitive) || file->displayPath().contains(m_searchText, Qt::CaseInsensitive);
    }

    return passesTypeFilter && passesSearchFilter;
}

// ============================================================================
// GitCommitDialog Implementation
// ============================================================================

GitCommitDialog::GitCommitDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_isAmendMode(false), m_isAllowEmpty(false), m_fileModel(std::make_unique<GitFileModel>(this)), m_proxyModel(std::make_unique<GitFileProxyModel>(this))
{
    setWindowTitle(tr("Git Commit"));
    setMinimumSize(800, 700);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(1200, 800);
    setAttribute(Qt::WA_DeleteOnClose);

    // Setup models
    m_proxyModel->setSourceModel(m_fileModel.get());

    setupUI();
    setupFileView();
    setupContextMenu();
    loadChangedFiles();

    qDebug() << "[GitCommitDialog] Initialized for repository:" << repositoryPath;
}

GitCommitDialog::GitCommitDialog(const QString &repositoryPath, const QStringList &files, QWidget *parent)
    : GitCommitDialog(repositoryPath, parent)
{
    // This constructor is for when specific files are passed
    // We'll still load all changed files but can highlight the specified ones
    Q_UNUSED(files)   // TODO: Implement file-specific logic if needed
}

void GitCommitDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // === Options Section ===
    m_optionsGroup = new QGroupBox(tr("Commit Options"), this);
    auto *optionsLayout = new QVBoxLayout(m_optionsGroup);

    m_optionsLabel = new QLabel(tr("Select commit type and options:"), this);
    m_optionsLabel->setStyleSheet("color: #666; font-size: 11px;");
    optionsLayout->addWidget(m_optionsLabel);

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

    // === Main Splitter ===
    m_mainSplitter = new QSplitter(Qt::Vertical, this);

    // === Message Section ===
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

    // === Files Section ===
    m_filesGroup = new QGroupBox(tr("Changed Files"), this);
    auto *filesLayout = new QVBoxLayout(m_filesGroup);

    // Filter and search toolbar
    auto *filterLayout = new QHBoxLayout();

    filterLayout->addWidget(new QLabel(tr("Filter:"), this));
    m_fileFilterCombo = new QComboBox(this);
    m_fileFilterCombo->addItem(tr("All files"), static_cast<int>(GitFileProxyModel::AllFiles));
    m_fileFilterCombo->addItem(tr("Staged files"), static_cast<int>(GitFileProxyModel::StagedFiles));
    m_fileFilterCombo->addItem(tr("Modified files"), static_cast<int>(GitFileProxyModel::ModifiedFiles));
    m_fileFilterCombo->addItem(tr("Untracked files"), static_cast<int>(GitFileProxyModel::UntrackedFiles));
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

    // Status statistics labels
    auto *statsLayout = new QHBoxLayout();
    m_stagedCountLabel = new QLabel(this);
    m_modifiedCountLabel = new QLabel(this);
    m_untrackedCountLabel = new QLabel(this);
    statsLayout->addWidget(m_stagedCountLabel);
    statsLayout->addWidget(m_modifiedCountLabel);
    statsLayout->addWidget(m_untrackedCountLabel);
    statsLayout->addStretch();
    filesLayout->addLayout(statsLayout);

    // File view will be added in setupFileView()
    filesLayout->addWidget(createFileViewPlaceholder());

    // File operation buttons
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

    // Set splitter proportions
    m_mainSplitter->setStretchFactor(0, 0);   // Message area fixed height
    m_mainSplitter->setStretchFactor(1, 1);   // Files area expandable

    mainLayout->addWidget(m_mainSplitter);

    // === Button Section ===
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

    // === Signal Connections ===
    connect(m_messageEdit, &QTextEdit::textChanged, this, &GitCommitDialog::onMessageChanged);
    connect(m_amendCheckBox, &QCheckBox::toggled, this, &GitCommitDialog::onAmendToggled);
    connect(m_allowEmptyCheckBox, &QCheckBox::toggled, this, &GitCommitDialog::onAllowEmptyToggled);
    connect(m_fileFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GitCommitDialog::onFilterChanged);
    connect(m_fileSearchEdit, &QLineEdit::textChanged, this, &GitCommitDialog::onFilterChanged);
    connect(m_refreshButton, &QPushButton::clicked, this, &GitCommitDialog::onRefreshFiles);
    connect(m_stageSelectedButton, &QPushButton::clicked, this, &GitCommitDialog::onStageSelected);
    connect(m_unstageSelectedButton, &QPushButton::clicked, this, &GitCommitDialog::onUnstageSelected);
    connect(m_selectAllButton, &QPushButton::clicked, this, &GitCommitDialog::onSelectAll);
    connect(m_selectNoneButton, &QPushButton::clicked, this, &GitCommitDialog::onSelectNone);
    connect(m_cancelButton, &QPushButton::clicked, this, &GitCommitDialog::onCancelClicked);
    connect(m_commitButton, &QPushButton::clicked, this, &GitCommitDialog::onCommitClicked);

    // Model signals
    connect(m_fileModel.get(), &GitFileModel::fileCheckStateChanged, this, &GitCommitDialog::onFileCheckStateChanged);

    qDebug() << "[GitCommitDialog] Enhanced UI setup completed";
}

QWidget *GitCommitDialog::createFileViewPlaceholder()
{
    // Temporary placeholder - will be replaced in setupFileView()
    auto *placeholder = new QWidget(this);
    placeholder->setMinimumHeight(200);
    return placeholder;
}

void GitCommitDialog::setupFileView()
{
    // Remove placeholder if it exists
    auto *filesLayout = qobject_cast<QVBoxLayout *>(m_filesGroup->layout());
    if (filesLayout && filesLayout->count() > 2) {
        auto *placeholder = filesLayout->itemAt(2)->widget();
        if (placeholder) {
            filesLayout->removeWidget(placeholder);
            placeholder->deleteLater();
        }
    }

    m_fileView = new QTreeView(this);
    m_fileView->setModel(m_proxyModel.get());
    m_fileView->setRootIsDecorated(false);
    m_fileView->setAlternatingRowColors(true);
    m_fileView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileView->setSortingEnabled(true);
    m_fileView->setContextMenuPolicy(Qt::CustomContextMenu);

    // Disable editing - files should not be editable in the list
    m_fileView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Configure header with better column widths
    auto *header = m_fileView->header();
    header->setStretchLastSection(false);   // Don't stretch last section initially
    header->setSectionResizeMode(0, QHeaderView::Interactive);   // File name - user can resize
    header->setSectionResizeMode(1, QHeaderView::Interactive);   // Status - user can resize
    header->setSectionResizeMode(2, QHeaderView::Stretch);   // Path - stretches to fill

    // Add to layout
    if (filesLayout) {
        filesLayout->insertWidget(2, m_fileView);
    }

    // Set initial column widths after adding to layout - delay this to ensure proper sizing
    QTimer::singleShot(0, this, [this]() {
        auto *header = m_fileView->header();
        header->resizeSection(0, 250);   // File name column - increased width
        header->resizeSection(1, 180);   // Status column - increased width
        header->setSectionResizeMode(2, QHeaderView::Stretch);   // Path - stretches to fill
        // Path column will stretch automatically
        qDebug() << "[GitCommitDialog] Set initial column widths: File=250, Status=180";
    });

    // Connect signals
    connect(m_fileView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &GitCommitDialog::onFileSelectionChanged);
    connect(m_fileView, &QTreeView::customContextMenuRequested,
            this, &GitCommitDialog::onShowContextMenu);
    connect(m_fileView, &QTreeView::doubleClicked,
            this, &GitCommitDialog::onFileDoubleClicked);
}

void GitCommitDialog::setupContextMenu()
{
    m_contextMenu = new QMenu(this);

    // === File Operation Actions ===
    m_stageAction = m_contextMenu->addAction(QIcon::fromTheme("list-add"), tr("Stage"));
    m_unstageAction = m_contextMenu->addAction(QIcon::fromTheme("list-remove"), tr("Unstage"));
    m_contextMenu->addSeparator();

    m_discardAction = m_contextMenu->addAction(QIcon::fromTheme("edit-undo"), tr("Discard Changes"));
    m_showDiffAction = m_contextMenu->addAction(QIcon::fromTheme("document-properties"), tr("Show Diff"));

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

    // === Connect signals ===
    connect(m_stageAction, &QAction::triggered, this, &GitCommitDialog::stageSelectedFiles);
    connect(m_unstageAction, &QAction::triggered, this, &GitCommitDialog::unstageSelectedFiles);
    connect(m_discardAction, &QAction::triggered, this, &GitCommitDialog::discardSelectedFiles);
    connect(m_showDiffAction, &QAction::triggered, this, &GitCommitDialog::showSelectedFilesDiff);

    // File management actions
    connect(openFileAction, &QAction::triggered, this, [this]() {
        auto filePaths = getSelectedFilePaths();
        if (!filePaths.isEmpty()) {
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePaths.first());
            GitDialogManager::instance()->openFile(absolutePath, this);
        }
    });

    connect(showFolderAction, &QAction::triggered, this, [this]() {
        auto filePaths = getSelectedFilePaths();
        if (!filePaths.isEmpty()) {
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePaths.first());
            GitDialogManager::instance()->showFileInFolder(absolutePath, this);
        }
    });

    // Git history actions
    connect(showLogAction, &QAction::triggered, this, [this]() {
        auto filePaths = getSelectedFilePaths();
        if (!filePaths.isEmpty()) {
            GitDialogManager::instance()->showLogDialog(m_repositoryPath, filePaths.first(), this);
        }
    });

    connect(showBlameAction, &QAction::triggered, this, [this]() {
        auto filePaths = getSelectedFilePaths();
        if (!filePaths.isEmpty()) {
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePaths.first());
            GitDialogManager::instance()->showBlameDialog(m_repositoryPath, absolutePath, this);
        }
    });

    // Advanced actions
    connect(copyPathAction, &QAction::triggered, this, [this]() {
        auto filePaths = getSelectedFilePaths();
        if (!filePaths.isEmpty()) {
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePaths.first());
            QApplication::clipboard()->setText(absolutePath);
            qDebug() << "[GitCommitDialog] Copied file path to clipboard:" << absolutePath;
        }
    });

    connect(copyNameAction, &QAction::triggered, this, [this]() {
        auto filePaths = getSelectedFilePaths();
        if (!filePaths.isEmpty()) {
            QString fileName = QFileInfo(filePaths.first()).fileName();
            QApplication::clipboard()->setText(fileName);
            qDebug() << "[GitCommitDialog] Copied file name to clipboard:" << fileName;
        }
    });

    connect(deleteFileAction, &QAction::triggered, this, [this]() {
        auto filePaths = getSelectedFilePaths();
        if (!filePaths.isEmpty()) {
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePaths.first());
            GitDialogManager::instance()->deleteFile(absolutePath, this);
            // Refresh after potential deletion
            QTimer::singleShot(100, this, [this]() {
                loadChangedFiles();
            });
        }
    });
}

void GitCommitDialog::loadChangedFiles()
{
    // 使用新的GitStatusParser来加载文件状态
    auto files = GitStatusParser::getRepositoryStatus(m_repositoryPath);
    
    // 转换GitFileInfo到GitFileItem
    QList<std::shared_ptr<GitFileItem>> gitFileItems;
    for (const auto &fileInfo : files) {
        GitFileItem::Status status;
        
        // 将GitFileStatus转换为GitFileItem::Status
        switch (fileInfo->status) {
        case GitFileStatus::Modified:
            status = GitFileItem::Status::Modified;
            break;
        case GitFileStatus::Staged:
            status = GitFileItem::Status::Staged;
            break;
        case GitFileStatus::StagedModified:
            status = GitFileItem::Status::StagedModified;
            break;
        case GitFileStatus::StagedAdded:
            status = GitFileItem::Status::StagedAdded;
            break;
        case GitFileStatus::StagedDeleted:
            status = GitFileItem::Status::StagedDeleted;
            break;
        case GitFileStatus::Deleted:
            status = GitFileItem::Status::Deleted;
            break;
        case GitFileStatus::Untracked:
            status = GitFileItem::Status::Untracked;
            break;
        case GitFileStatus::Renamed:
            status = GitFileItem::Status::Renamed;
            break;
        case GitFileStatus::Copied:
            status = GitFileItem::Status::Copied;
            break;
        default:
            status = GitFileItem::Status::Modified;
            break;
        }
        
        auto gitFileItem = std::make_shared<GitFileItem>(fileInfo->filePath, status, fileInfo->statusText);
        gitFileItems.append(gitFileItem);
    }

    m_fileModel->setFiles(gitFileItems);
    updateFileCountLabels();
    updateButtonStates();

    qDebug() << "[GitCommitDialog] Loaded" << gitFileItems.size() << "changed files using GitStatusParser";
}

void GitCommitDialog::updateFileCountLabels()
{
    int stagedCount = 0, modifiedCount = 0, untrackedCount = 0;

    auto files = m_fileModel->getAllFiles();
    for (const auto &file : files) {
        if (file->isStaged()) {
            stagedCount++;
        } else if (file->status() == GitFileItem::Status::Untracked) {
            untrackedCount++;
        } else {
            modifiedCount++;
        }
    }

    m_stagedCountLabel->setText(tr("Staged: %1").arg(stagedCount));
    m_stagedCountLabel->setStyleSheet(stagedCount > 0 ? "color: #4CAF50; font-size: 11px;" : "color: #666; font-size: 11px;");

    m_modifiedCountLabel->setText(tr("Modified: %1").arg(modifiedCount));
    m_modifiedCountLabel->setStyleSheet(modifiedCount > 0 ? "color: #FF9800; font-size: 11px;" : "color: #666; font-size: 11px;");

    m_untrackedCountLabel->setText(tr("Untracked: %1").arg(untrackedCount));
    m_untrackedCountLabel->setStyleSheet(untrackedCount > 0 ? "color: #2196F3; font-size: 11px;" : "color: #666; font-size: 11px;");
}

void GitCommitDialog::updateButtonStates()
{
    bool hasSelection = m_fileView->selectionModel()->hasSelection();
    auto checkedFiles = m_fileModel->getCheckedFiles();
    bool hasCheckedFiles = !checkedFiles.isEmpty();
    bool hasMessage = !m_messageEdit->toPlainText().trimmed().isEmpty();

    m_stageSelectedButton->setEnabled(hasSelection);
    m_unstageSelectedButton->setEnabled(hasSelection);

    // Commit button: need checked files (or allow empty) and message
    m_commitButton->setEnabled((hasCheckedFiles || m_isAllowEmpty) && hasMessage);
}

void GitCommitDialog::loadLastCommitMessage()
{
    if (!m_isAmendMode) {
        return;
    }

    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    process.start("git", QStringList() << "log"
                                       << "-1"
                                       << "--pretty=format:%B");
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

    // Check if there are files to commit (unless allowing empty)
    auto checkedFiles = m_fileModel->getCheckedFiles();
    if (!m_isAllowEmpty && checkedFiles.isEmpty()) {
        QMessageBox::warning(this, tr("No Files Selected"),
                             tr("Please select files to commit, or enable 'Allow empty commit'."));
        return false;
    }

    return true;
}

void GitCommitDialog::commitChanges()
{
    if (!validateCommitMessage()) {
        return;
    }

    // First, ensure all checked files are properly staged
    auto checkedFiles = m_fileModel->getCheckedFiles();
    if (!m_isAllowEmpty && checkedFiles.isEmpty()) {
        QMessageBox::warning(this, tr("No Files Selected"),
                             tr("Please select files to commit, or enable 'Allow empty commit'."));
        return;
    }

    // Stage checked files that are not already staged
    bool needsStaging = false;
    for (const auto &file : checkedFiles) {
        if (!file->isStaged()) {
            needsStaging = true;
            break;
        }
    }

    if (needsStaging) {
        QProgressDialog progress(tr("Staging files for commit..."), tr("Cancel"), 0, checkedFiles.size(), this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();

        int count = 0;
        for (const auto &file : checkedFiles) {
            if (progress.wasCanceled()) {
                return;
            }

            if (!file->isStaged()) {
                progress.setLabelText(tr("Staging: %1").arg(file->fileName()));
                progress.setValue(count);

                stageFile(file->filePath());
                QApplication::processEvents();
            }
            count++;
        }
        progress.setValue(checkedFiles.size());

        // Small delay to ensure git operations complete
        QThread::msleep(100);
    }

    const QString message = getCommitMessage();

    // Check for existing git lock file and try to clean it up
    QString lockFilePath = m_repositoryPath + "/.git/index.lock";
    if (QFile::exists(lockFilePath)) {
        int ret = QMessageBox::warning(this, tr("Git Lock File"),
                                       tr("A Git lock file exists. This may indicate another Git process is running.\n\n"
                                          "Do you want to remove the lock file and continue?"),
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            QFile::remove(lockFilePath);
            qDebug() << "[GitCommitDialog] Removed git lock file:" << lockFilePath;
        } else {
            return;
        }
    }

    // Build Git command arguments
    QStringList args;
    args << "commit"
         << "-m" << message;

    if (m_isAmendMode) {
        args << "--amend";
    }

    if (m_isAllowEmpty) {
        args << "--allow-empty";
    }

    qDebug() << "[GitCommitDialog] Executing commit with args:" << args;

    // Use GitOperationDialog to execute commit
    auto *operationDialog = new GitOperationDialog("Commit", this);
    operationDialog->setAttribute(Qt::WA_DeleteOnClose);
    operationDialog->setOperationDescription(tr("Committing changes to repository..."));

    connect(operationDialog, &QDialog::accepted, [this, operationDialog]() {
        auto result = operationDialog->getExecutionResult();
        if (result == GitCommandExecutor::Result::Success) {
            qDebug() << "[GitCommitDialog] Commit completed successfully";
            accept();
        } else {
            qWarning() << "[GitCommitDialog] Commit failed";
        }
    });

    operationDialog->executeCommand(m_repositoryPath, args);
    operationDialog->show();
}

void GitCommitDialog::stageFile(const QString &filePath)
{
    auto result = GitOperationUtils::stageFile(m_repositoryPath, filePath);
    if (result.success) {
        qDebug() << "[GitCommitDialog] Successfully staged file:" << filePath;
    } else {
        qWarning() << "[GitCommitDialog] Failed to stage file:" << filePath << result.error;
    }
}

void GitCommitDialog::unstageFile(const QString &filePath)
{
    auto result = GitOperationUtils::unstageFile(m_repositoryPath, filePath);
    if (result.success) {
        qDebug() << "[GitCommitDialog] Successfully unstaged file:" << filePath;
    } else {
        qWarning() << "[GitCommitDialog] Failed to unstage file:" << filePath << result.error;
    }
}

void GitCommitDialog::discardFile(const QString &filePath)
{
    auto result = GitOperationUtils::discardFile(m_repositoryPath, filePath);
    if (result.success) {
        qDebug() << "[GitCommitDialog] Successfully discarded changes for file:" << filePath;
    } else {
        qWarning() << "[GitCommitDialog] Failed to discard changes for file:" << filePath << result.error;
    }
}

void GitCommitDialog::showFileDiff(const QString &filePath)
{
    if (filePath.isEmpty()) {
        QMessageBox::information(this, tr("No File"), tr("Please select a file to view diff."));
        return;
    }

    // Use GitDialogManager to show diff dialog
    GitDialogManager::instance()->showDiffDialog(m_repositoryPath, filePath, this);

    qDebug() << "[GitCommitDialog] Opened diff dialog for file:" << filePath;
}

QString GitCommitDialog::getCommitMessage() const
{
    return m_messageEdit->toPlainText().trimmed();
}

QStringList GitCommitDialog::getSelectedFiles() const
{
    QStringList files;
    auto checkedFiles = m_fileModel->getCheckedFiles();

    for (const auto &file : checkedFiles) {
        files.append(file->filePath());
    }

    return files;
}

QStringList GitCommitDialog::getSelectedFilePaths() const
{
    QStringList filePaths;
    auto selectedIndexes = m_fileView->selectionModel()->selectedRows();

    for (const auto &index : selectedIndexes) {
        auto sourceIndex = m_proxyModel->mapToSource(index);
        auto file = sourceIndex.data(GitFileModel::FileItemRole).value<std::shared_ptr<GitFileItem>>();
        if (file) {
            filePaths.append(file->filePath());
        }
    }

    return filePaths;
}

bool GitCommitDialog::isAmendMode() const
{
    return m_isAmendMode;
}

bool GitCommitDialog::isAllowEmpty() const
{
    return m_isAllowEmpty;
}

// ============================================================================
// Slot Implementations
// ============================================================================

void GitCommitDialog::onCommitClicked()
{
    commitChanges();
}

void GitCommitDialog::onCancelClicked()
{
    reject();
}

void GitCommitDialog::onMessageChanged()
{
    updateButtonStates();
}

void GitCommitDialog::onAmendToggled(bool enabled)
{
    m_isAmendMode = enabled;

    if (enabled) {
        loadLastCommitMessage();
    } else {
        m_messageEdit->clear();
    }

    updateButtonStates();
    qDebug() << "[GitCommitDialog] Amend mode:" << (enabled ? "enabled" : "disabled");
}

void GitCommitDialog::onAllowEmptyToggled(bool enabled)
{
    m_isAllowEmpty = enabled;
    updateButtonStates();
    qDebug() << "[GitCommitDialog] Allow empty commit:" << (enabled ? "enabled" : "disabled");
}

void GitCommitDialog::onFileCheckStateChanged(const QString &filePath, bool checked)
{
    Q_UNUSED(filePath)
    Q_UNUSED(checked)
    // File check state changed, update button states
    updateButtonStates();
}

void GitCommitDialog::onFileSelectionChanged()
{
    updateButtonStates();
}

void GitCommitDialog::onFilterChanged()
{
    auto filterType = static_cast<GitFileProxyModel::FilterType>(
            m_fileFilterCombo->currentData().toInt());
    m_proxyModel->setFilterType(filterType);
    m_proxyModel->setSearchText(m_fileSearchEdit->text());
}

void GitCommitDialog::onRefreshFiles()
{
    loadChangedFiles();
}

void GitCommitDialog::onStageSelected()
{
    stageSelectedFiles();
}

void GitCommitDialog::onUnstageSelected()
{
    unstageSelectedFiles();
}

void GitCommitDialog::onSelectAll()
{
    m_fileView->selectAll();
}

void GitCommitDialog::onSelectNone()
{
    m_fileView->clearSelection();
}

void GitCommitDialog::onShowContextMenu(const QPoint &pos)
{
    QModelIndex index = m_fileView->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    auto selectedIndexes = m_fileView->selectionModel()->selectedRows();
    if (selectedIndexes.isEmpty()) {
        return;
    }

    // Analyze selected files to determine menu state
    bool hasStaged = false;
    bool hasUnstaged = false;
    bool hasModified = false;
    bool hasUntracked = false;

    for (const auto &selectedIndex : selectedIndexes) {
        auto sourceIndex = m_proxyModel->mapToSource(selectedIndex);
        auto file = sourceIndex.data(GitFileModel::FileItemRole).value<std::shared_ptr<GitFileItem>>();
        if (file) {
            if (file->isStaged()) {
                hasStaged = true;
            } else {
                hasUnstaged = true;
                if (file->status() == GitFileItem::Status::Untracked) {
                    hasUntracked = true;
                } else {
                    hasModified = true;
                }
            }
        }
    }

    // Update menu actions based on selection analysis
    m_stageAction->setEnabled(hasUnstaged);
    m_stageAction->setText(hasUntracked ? tr("Add to Git") : tr("Stage"));

    m_unstageAction->setEnabled(hasStaged);
    m_discardAction->setEnabled(hasModified);   // Only enable for modified files, not untracked
    m_showDiffAction->setEnabled(hasModified || hasStaged);   // Don't show diff for untracked files

    // Update tooltips for better UX
    if (hasUntracked && hasModified) {
        m_stageAction->setToolTip(tr("Add untracked files and stage modified files"));
    } else if (hasUntracked) {
        m_stageAction->setToolTip(tr("Add untracked files to Git"));
    } else if (hasModified) {
        m_stageAction->setToolTip(tr("Stage modified files"));
    }

    m_contextMenu->exec(m_fileView->mapToGlobal(pos));
}

void GitCommitDialog::onFileDoubleClicked(const QModelIndex &index)
{
    auto sourceIndex = m_proxyModel->mapToSource(index);
    auto file = sourceIndex.data(GitFileModel::FileItemRole).value<std::shared_ptr<GitFileItem>>();
    if (file) {
        // 根据文件状态决定双击行为
        if (file->status() == GitFileItem::Status::Untracked) {
            // 未跟踪文件 - 直接打开文件查看内容
            QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(file->filePath());
            GitDialogManager::instance()->openFile(absolutePath, this);
        } else {
            // 已跟踪文件（修改、暂存等）- 显示差异
            showFileDiff(file->filePath());
        }
    }
}

// ============================================================================
// Context Menu Action Implementations
// ============================================================================

void GitCommitDialog::stageSelectedFiles()
{
    auto selectedIndexes = m_fileView->selectionModel()->selectedRows();

    QStringList filesToStage;
    for (const auto &index : selectedIndexes) {
        auto sourceIndex = m_proxyModel->mapToSource(index);
        auto file = sourceIndex.data(GitFileModel::FileItemRole).value<std::shared_ptr<GitFileItem>>();
        if (file && !file->isStaged()) {
            filesToStage.append(file->filePath());
        }
    }

    if (filesToStage.isEmpty()) {
        QMessageBox::information(this, tr("No Files to Stage"),
                                 tr("Selected files are already staged."));
        return;
    }

    QProgressDialog progress(tr("Staging files..."), tr("Cancel"), 0, filesToStage.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    bool success = true;
    for (int i = 0; i < filesToStage.size(); ++i) {
        if (progress.wasCanceled()) {
            break;
        }

        const QString &filePath = filesToStage.at(i);
        progress.setLabelText(tr("Staging: %1").arg(QFileInfo(filePath).fileName()));
        progress.setValue(i);

        stageFile(filePath);
        QApplication::processEvents();
    }

    progress.setValue(filesToStage.size());

    if (success) {
        // Refresh file list to reflect changes
        QTimer::singleShot(200, this, [this]() {
            loadChangedFiles();
        });
    }
}

void GitCommitDialog::unstageSelectedFiles()
{
    auto selectedIndexes = m_fileView->selectionModel()->selectedRows();

    QStringList filesToUnstage;
    for (const auto &index : selectedIndexes) {
        auto sourceIndex = m_proxyModel->mapToSource(index);
        auto file = sourceIndex.data(GitFileModel::FileItemRole).value<std::shared_ptr<GitFileItem>>();
        if (file && file->isStaged()) {
            filesToUnstage.append(file->filePath());
        }
    }

    if (filesToUnstage.isEmpty()) {
        QMessageBox::information(this, tr("No Files to Unstage"),
                                 tr("Selected files are not staged."));
        return;
    }

    QProgressDialog progress(tr("Unstaging files..."), tr("Cancel"), 0, filesToUnstage.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    bool success = true;
    for (int i = 0; i < filesToUnstage.size(); ++i) {
        if (progress.wasCanceled()) {
            break;
        }

        const QString &filePath = filesToUnstage.at(i);
        progress.setLabelText(tr("Unstaging: %1").arg(QFileInfo(filePath).fileName()));
        progress.setValue(i);

        unstageFile(filePath);
        QApplication::processEvents();
    }

    progress.setValue(filesToUnstage.size());

    if (success) {
        // Refresh file list to reflect changes
        QTimer::singleShot(200, this, [this]() {
            loadChangedFiles();
        });
    }
}

void GitCommitDialog::discardSelectedFiles()
{
    auto selectedIndexes = m_fileView->selectionModel()->selectedRows();
    QStringList filesToDiscard;

    for (const auto &index : selectedIndexes) {
        auto sourceIndex = m_proxyModel->mapToSource(index);
        auto file = sourceIndex.data(GitFileModel::FileItemRole).value<std::shared_ptr<GitFileItem>>();
        if (file && !file->isStaged() && file->status() != GitFileItem::Status::Untracked) {
            filesToDiscard.append(file->filePath());
        }
    }

    if (filesToDiscard.isEmpty()) {
        QMessageBox::information(this, tr("No Files to Discard"),
                                 tr("No modified files selected for discarding."));
        return;
    }

    int ret = QMessageBox::warning(this, tr("Discard Changes"),
                                   tr("Are you sure you want to discard changes to %1 file(s)?\n\n"
                                      "This action cannot be undone and will permanently lose your changes.")
                                           .arg(filesToDiscard.size()),
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        QProgressDialog progress(tr("Discarding changes..."), tr("Cancel"), 0, filesToDiscard.size(), this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();

        for (int i = 0; i < filesToDiscard.size(); ++i) {
            if (progress.wasCanceled()) {
                break;
            }

            const QString &filePath = filesToDiscard.at(i);
            progress.setLabelText(tr("Discarding: %1").arg(QFileInfo(filePath).fileName()));
            progress.setValue(i);

            discardFile(filePath);
            QApplication::processEvents();
        }

        progress.setValue(filesToDiscard.size());

        // Refresh file list to reflect changes
        QTimer::singleShot(200, this, [this]() {
            loadChangedFiles();
        });
    }
}

void GitCommitDialog::showSelectedFilesDiff()
{
    auto selectedIndexes = m_fileView->selectionModel()->selectedRows();

    for (const auto &index : selectedIndexes) {
        auto sourceIndex = m_proxyModel->mapToSource(index);
        auto file = sourceIndex.data(GitFileModel::FileItemRole).value<std::shared_ptr<GitFileItem>>();
        if (file) {
            showFileDiff(file->filePath());
            break;   // Show diff for first selected file
        }
    }
}
