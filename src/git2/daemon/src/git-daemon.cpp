#include "git-daemon.h"
#include "git-service.h"
#include "git-repository-watcher.h"
#include "git-status-cache.h"
#include <QDebug>

GitDaemon::GitDaemon(QObject *parent)
    : QObject(parent)
    , m_service(nullptr)
    , m_watcher(nullptr)
    , m_healthTimer(new QTimer(this))
    , m_initialized(false)
{
    qDebug() << "[GitDaemon] Constructor called";
}

GitDaemon::~GitDaemon()
{
    shutdown();
    qDebug() << "[GitDaemon] Destructor called";
}

bool GitDaemon::initialize()
{
    if (m_initialized) {
        qWarning() << "[GitDaemon::initialize] Already initialized";
        return true;
    }
    
    qDebug() << "[GitDaemon::initialize] Initializing daemon components";
    
    // 创建服务组件
    m_service = new GitService(this);
    if (!m_service) {
        qCritical() << "[GitDaemon::initialize] Failed to create GitService";
        return false;
    }
    
    // 创建仓库监控器
    m_watcher = new GitRepositoryWatcher(this);
    if (!m_watcher) {
        qCritical() << "[GitDaemon::initialize] Failed to create GitRepositoryWatcher";
        return false;
    }
    
    // 设置组件间连接
    setupConnections();
    
    // 设置健康检查定时器
    m_healthTimer->setInterval(30000); // 30秒检查一次
    connect(m_healthTimer, &QTimer::timeout, this, &GitDaemon::onHealthCheck);
    m_healthTimer->start();
    
    m_initialized = true;
    qDebug() << "[GitDaemon::initialize] Daemon initialization completed successfully";
    
    return true;
}

void GitDaemon::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    qDebug() << "[GitDaemon::shutdown] Shutting down daemon";
    
    // 停止健康检查
    if (m_healthTimer) {
        m_healthTimer->stop();
    }
    
    // 清理组件
    if (m_watcher) {
        m_watcher->deleteLater();
        m_watcher = nullptr;
    }
    
    if (m_service) {
        m_service->deleteLater();
        m_service = nullptr;
    }
    
    m_initialized = false;
    qDebug() << "[GitDaemon::shutdown] Daemon shutdown completed";
}

void GitDaemon::setupConnections()
{
    // 连接仓库监控器和服务
    connect(m_watcher, &GitRepositoryWatcher::repositoryChanged,
            this, &GitDaemon::onRepositoryChanged);
    
    connect(m_watcher, &GitRepositoryWatcher::repositoryDiscovered,
            m_service, &GitService::RegisterRepository);
    
    qDebug() << "[GitDaemon::setupConnections] Component connections established";
}

void GitDaemon::onRepositoryChanged(const QString &repositoryPath)
{
    qDebug() << "[GitDaemon::onRepositoryChanged] Repository changed:" << repositoryPath;
    
    // 通知服务刷新仓库状态
    if (m_service) {
        m_service->RefreshRepository(repositoryPath);
    }
}

void GitDaemon::onHealthCheck()
{
    // 执行基本的健康检查
    int cacheSize = GitStatusCache::instance().getCacheSize();
    QStringList repositories = GitStatusCache::instance().getCachedRepositories();
    
    qDebug() << "[GitDaemon::onHealthCheck] Health check - Cache size:" << cacheSize 
             << "Repositories:" << repositories.size();
    
    // 如果缓存过大，触发清理
    if (cacheSize > 50000) { // 50k entries
        qWarning() << "[GitDaemon::onHealthCheck] Cache size too large, triggering cleanup";
        GitStatusCache::instance().performCleanup();
    }
}
