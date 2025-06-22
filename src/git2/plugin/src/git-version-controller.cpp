#include "git-version-controller.h"
#include "git-dbus-client.h"
#include <QDebug>
#include <QThread>
#include <QCoreApplication>

GitVersionController::GitVersionController(QObject *parent)
    : QObject(parent)
    , m_dbusClient(&GitDBusClient::instance())
    , m_timer(nullptr)
    , m_useFileSystemWatcher(true)
{
    Q_ASSERT(qApp->thread() == QThread::currentThread());
    
    qInfo() << "INFO: [GitVersionController] Initializing with DBus client integration";

    // 创建定时器作为备用机制
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(5000); // 5秒延迟
    connect(m_timer, &QTimer::timeout, this, &GitVersionController::onTimeout);

    // 连接DBus客户端信号
    connect(m_dbusClient, &GitDBusClient::repositoryStatusChanged,
            this, [this](const QString &repositoryPath, const QVariantMap &changes) {
                Q_UNUSED(changes)
                qDebug() << "[GitVersionController] Repository status changed:" << repositoryPath;
            });

    connect(m_dbusClient, &GitDBusClient::repositoryDiscovered,
            this, &GitVersionController::onNewRepositoryAdded);

    qInfo() << "INFO: [GitVersionController] DBus client integration enabled";
}

GitVersionController::~GitVersionController()
{
    if (m_timer) {
        m_timer->stop();
    }
    qDebug() << "[GitVersionController] Controller destroyed";
}

void GitVersionController::onNewRepositoryAdded(const QString &repositoryPath)
{
    qInfo() << "INFO: [GitVersionController] New repository added:" << repositoryPath;
    
    // 注册仓库到DBus服务
    bool success = m_dbusClient->registerRepository(repositoryPath);
    if (success) {
        qDebug() << "[GitVersionController] Successfully registered repository:" << repositoryPath;
        
        // 触发初始状态检索
        const QUrl url = QUrl::fromLocalFile(repositoryPath);
        if (url.isValid()) {
            Q_EMIT requestRetrieval(url);
            qDebug() << "[GitVersionController] Triggered initial retrieval for:" << repositoryPath;
        }
    } else {
        qWarning() << "[GitVersionController] Failed to register repository:" << repositoryPath;
    }
}

void GitVersionController::onRepositoryChanged(const QString &repositoryPath)
{
    qDebug() << "[GitVersionController] Repository changed:" << repositoryPath;
    
    // 通过DBus请求刷新仓库状态
    bool success = m_dbusClient->refreshRepository(repositoryPath);
    if (success) {
        qDebug() << "[GitVersionController] Successfully requested refresh for:" << repositoryPath;
    } else {
        qWarning() << "[GitVersionController] Failed to request refresh for:" << repositoryPath;
    }
    
    // 同时触发本地检索作为备用
    const QUrl url = QUrl::fromLocalFile(repositoryPath);
    if (url.isValid()) {
        Q_EMIT requestRetrieval(url);
    }
}

void GitVersionController::onRepositoryUpdateRequested(const QString &repositoryPath)
{
    qInfo() << "INFO: [GitVersionController] Repository update requested from service:" << repositoryPath;

    // 通过DBus触发状态检索
    bool success = m_dbusClient->triggerRetrieval(repositoryPath);
    if (success) {
        qDebug() << "[GitVersionController] Successfully triggered DBus retrieval for:" << repositoryPath;
    } else {
        qWarning() << "[GitVersionController] Failed to trigger DBus retrieval for:" << repositoryPath;
    }
    
    // 同时发出本地信号作为备用
    const QUrl url = QUrl::fromLocalFile(repositoryPath);
    if (url.isValid()) {
        Q_EMIT requestRetrieval(url);
        qDebug() << "[GitVersionController] Triggered local retrieval signal for repository:" << repositoryPath;
    }
}

void GitVersionController::onTimeout()
{
    qDebug() << "[GitVersionController] Timer timeout - performing backup status check";
    
    // 定时器超时时，检查服务状态
    if (m_dbusClient->isServiceAvailable()) {
        QVariantMap status = m_dbusClient->getServiceStatus();
        qDebug() << "[GitVersionController] Service status:" << status;
    } else {
        qWarning() << "[GitVersionController] DBus service not available during timeout check";
    }
} 