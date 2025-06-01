#ifndef GITREMOTEMANAGER_H
#define GITREMOTEMANAGER_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QProgressBar>
#include <QTimer>
#include <QMessageBox>
#include <QInputDialog>

class GitOperationService;

/**
 * @brief Git远程仓库管理器
 * 
 * 提供完整的远程仓库管理功能，包括：
 * - 远程仓库的增删改查
 * - 连接测试和状态验证
 * - 分支映射管理
 * - 批量操作支持
 */
class GitRemoteManager : public QDialog
{
    Q_OBJECT

public:
    explicit GitRemoteManager(const QString &repositoryPath, QWidget *parent = nullptr);
    ~GitRemoteManager();

    /**
     * @brief 远程仓库信息结构
     */
    struct RemoteInfo {
        QString name;                 // 远程仓库名称
        QString fetchUrl;             // 拉取URL
        QString pushUrl;              // 推送URL
        bool isConnected;             // 连接状态
        QString lastError;            // 最后错误信息
        QStringList branches;         // 远程分支列表
    };

private slots:
    void onRemoteSelectionChanged();
    void addRemote();
    void removeRemote();
    void editRemote();
    void testConnection();
    void testAllConnections();
    void refreshRemotes();
    void onOperationCompleted(bool success, const QString &message);

private:
    // === UI设置方法 ===
    void setupUI();
    void setupRemoteListGroup();
    void setupRemoteDetailsGroup();
    void setupButtonGroup();
    void setupConnections();

    // === 数据加载方法 ===
    void loadRemotes();
    void loadRemoteDetails(const QString &remoteName);
    void updateRemotesList();
    void updateRemoteDetails();

    // === 操作方法 ===
    void addNewRemote(const QString &name, const QString &url);
    void updateRemoteUrl(const QString &name, const QString &url);
    void deleteRemote(const QString &name);
    bool validateRemoteName(const QString &name) const;
    bool validateRemoteUrl(const QString &url) const;

    // === 辅助方法 ===
    void enableControls(bool enabled);
    void showProgress(const QString &message);
    void hideProgress();
    QString formatRemoteInfo(const RemoteInfo &info) const;

    // === 成员变量 ===
    QString m_repositoryPath;
    GitOperationService *m_operationService;

    // === UI组件 ===
    // 远程列表组
    QGroupBox *m_remoteListGroup;
    QListWidget *m_remotesWidget;
    QLabel *m_remotesCountLabel;

    // 远程详情组
    QGroupBox *m_detailsGroup;
    QLineEdit *m_nameEdit;
    QLineEdit *m_fetchUrlEdit;
    QLineEdit *m_pushUrlEdit;
    QLabel *m_connectionStatusLabel;
    QLabel *m_branchesCountLabel;
    QListWidget *m_branchesWidget;

    // 按钮组
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_editButton;
    QPushButton *m_testButton;
    QPushButton *m_testAllButton;
    QPushButton *m_refreshButton;
    QPushButton *m_closeButton;

    // 进度显示
    QProgressBar *m_progressBar;
    QLabel *m_progressLabel;

    // === 数据成员 ===
    QVector<RemoteInfo> m_remotes;
    QString m_selectedRemote;
    bool m_isOperationInProgress;
};

#endif // GITREMOTEMANAGER_H 