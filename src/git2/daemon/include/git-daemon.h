#pragma once

#include <QObject>
#include <QTimer>

class GitService;
class GitRepositoryWatcher;
class GitVersionWorker;

/**
 * @brief Git守护进程主管理类
 * 
 * 负责协调GitService和GitRepositoryWatcher的工作，
 * 管理daemon进程的生命周期和资源清理
 */
class GitDaemon : public QObject
{
    Q_OBJECT
    
public:
    explicit GitDaemon(QObject *parent = nullptr);
    ~GitDaemon();
    
    bool initialize();
    void shutdown();
    
    GitService* service() const { return m_service; }
    GitRepositoryWatcher* watcher() const { return m_watcher; }
    GitVersionWorker* versionWorker() const { return m_versionWorker; }
    
    void clearAllResources();
    
private Q_SLOTS:
    void onRepositoryChanged(const QString &repositoryPath);
    void onHealthCheck();
    void onClearAllResourcesRequested();
    
private:
    GitService *m_service;
    GitRepositoryWatcher *m_watcher;
    GitVersionWorker *m_versionWorker;
    QTimer *m_healthTimer;
    bool m_initialized;
    
    void setupConnections();
};
