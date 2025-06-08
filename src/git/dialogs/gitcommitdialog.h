#ifndef GITCOMMITDIALOG_H
#define GITCOMMITDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QSplitter>
#include <QLineEdit>
#include <QComboBox>
#include <QMenu>
#include <QStandardItemModel>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QKeyEvent>
#include <QEvent>
#include <memory>

// Forward declarations
class GitFileModel;
class GitFileProxyModel;
class GitFileItem;
class GitStatusParser;
class GitOperationUtils;
class GitFilePreviewDialog;

/**
 * @brief Represents a file in the Git repository with its status
 */
class GitFileItem
{
public:
    enum class Status {
        Modified,       // Modified but not staged
        Staged,         // Staged for commit
        Untracked,      // Not tracked by Git
        Deleted,        // Deleted but not staged
        StagedDeleted,  // Staged for deletion
        StagedModified, // Staged modification
        StagedAdded,    // Staged addition
        Renamed,        // Renamed file
        Copied          // Copied file
    };

    explicit GitFileItem(const QString &filePath, Status status, const QString &statusText = QString());

    QString filePath() const { return m_filePath; }
    QString fileName() const;
    QString displayPath() const { return m_filePath; }
    Status status() const { return m_status; }
    QString statusText() const { return m_statusText; }
    bool isStaged() const;
    bool isChecked() const { return m_checked; }
    void setChecked(bool checked) { m_checked = checked; }

    QIcon statusIcon() const;
    QString statusDisplayText() const;

private:
    QString m_filePath;
    Status m_status;
    QString m_statusText;
    bool m_checked;
};

/**
 * @brief Model for managing Git files in the commit dialog
 */
class GitFileModel : public QStandardItemModel
{
    Q_OBJECT

public:
    enum Roles {
        FileItemRole = Qt::UserRole + 1,
        FilePathRole,
        StatusRole,
        IsCheckedRole
    };

    explicit GitFileModel(QObject *parent = nullptr);

    void setFiles(const QList<std::shared_ptr<GitFileItem>> &files);
    void addFile(std::shared_ptr<GitFileItem> file);
    void updateFile(const QString &filePath, GitFileItem::Status status);
    void removeFile(const QString &filePath);
    void clear();

    std::shared_ptr<GitFileItem> getFileItem(const QString &filePath) const;
    QList<std::shared_ptr<GitFileItem>> getCheckedFiles() const;
    QList<std::shared_ptr<GitFileItem>> getAllFiles() const;

    // QStandardItemModel overrides
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

Q_SIGNALS:
    void fileCheckStateChanged(const QString &filePath, bool checked);

private:
    void updateModelItem(QStandardItem *item, std::shared_ptr<GitFileItem> fileItem);
    QStandardItem *findItemByPath(const QString &filePath) const;

    QList<std::shared_ptr<GitFileItem>> m_files;
};

/**
 * @brief Proxy model for filtering and sorting Git files
 */
class GitFileProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum FilterType {
        AllFiles,
        StagedFiles,
        ModifiedFiles,
        UntrackedFiles
    };

    explicit GitFileProxyModel(QObject *parent = nullptr);

    void setFilterType(FilterType type);
    void setSearchText(const QString &text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    FilterType m_filterType;
    QString m_searchText;
};

/**
 * @brief Modern Git commit dialog with proper architecture
 */
class GitCommitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitCommitDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    explicit GitCommitDialog(const QString &repositoryPath, const QStringList &files, QWidget *parent = nullptr);

    QString getCommitMessage() const;
    QStringList getSelectedFiles() const;
    bool isAmendMode() const;
    bool isAllowEmpty() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void onCommitClicked();
    void onCancelClicked();
    void onMessageChanged();
    void onAmendToggled(bool enabled);
    void onAllowEmptyToggled(bool enabled);
    void onFileCheckStateChanged(const QString &filePath, bool checked);
    void onFileSelectionChanged();
    void onFilterChanged();
    void onRefreshFiles();
    void onStageSelected();
    void onUnstageSelected();
    void onSelectAll();
    void onSelectNone();
    void onShowContextMenu(const QPoint &pos);
    void onFileDoubleClicked(const QModelIndex &index);
    void previewSelectedFile();

private:
    void setupUI();
    void setupFileView();
    void setupContextMenu();
    QWidget *createFileViewPlaceholder();
    void loadChangedFiles();
    void loadLastCommitMessage();
    void loadCommitTemplate();
    bool validateCommitMessage();
    void commitChanges();
    void updateFileCountLabels();
    void updateButtonStates();
    void stageFile(const QString &filePath);
    void unstageFile(const QString &filePath);
    void discardFile(const QString &filePath);
    void showFileDiff(const QString &filePath);

    // Helper methods for context menu
    QStringList getSelectedFilePaths() const;
    QString getCurrentSelectedFilePath() const;

    // Context menu actions
    void stageSelectedFiles();
    void unstageSelectedFiles();
    void discardSelectedFiles();
    void showSelectedFilesDiff();

    // Column width management
    void saveColumnWidths();
    void restoreColumnWidths();
    void setDefaultColumnWidths();

    QString m_repositoryPath;
    QString m_lastCommitMessage;
    QString m_commitTemplate;
    bool m_isAmendMode;
    bool m_isAllowEmpty;

    // Models
    std::unique_ptr<GitFileModel> m_fileModel;
    std::unique_ptr<GitFileProxyModel> m_proxyModel;

    // Column width preservation
    QList<int> m_savedColumnWidths;
    QList<QHeaderView::ResizeMode> m_savedResizeModes;

    // UI Components
    QSplitter *m_mainSplitter;

    // Options section
    QGroupBox *m_optionsGroup;
    QCheckBox *m_amendCheckBox;
    QCheckBox *m_allowEmptyCheckBox;
    QLabel *m_optionsLabel;

    // Message section
    QGroupBox *m_messageGroup;
    QTextEdit *m_messageEdit;
    QLabel *m_messageHintLabel;

    // Files section
    QGroupBox *m_filesGroup;
    QComboBox *m_fileFilterCombo;
    QLineEdit *m_fileSearchEdit;
    QTreeView *m_fileView;
    QLabel *m_stagedCountLabel;
    QLabel *m_modifiedCountLabel;
    QLabel *m_untrackedCountLabel;

    // Context menu
    QMenu *m_contextMenu;
    QAction *m_stageAction;
    QAction *m_unstageAction;
    QAction *m_discardAction;
    QAction *m_showDiffAction;
    QAction *m_previewAction;

    // Action buttons
    QPushButton *m_refreshButton;
    QPushButton *m_stageSelectedButton;
    QPushButton *m_unstageSelectedButton;
    QPushButton *m_selectAllButton;
    QPushButton *m_selectNoneButton;
    QPushButton *m_commitButton;
    QPushButton *m_cancelButton;
    
    // File preview
    GitFilePreviewDialog *m_currentPreviewDialog;
};

#endif // GITCOMMITDIALOG_H
