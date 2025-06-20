#include "git-dbus-client.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDebug>
#include <QFileInfo>
#include <QDir>

const QString GitDBusClient::SERVICE_NAME = "org.deepin.FileManager.Git";
const QString GitDBusClient::OBJECT_PATH = "/org/deepin/filemanager/git";
const QString GitDBusClient::INTERFACE_NAME = "org.deepin.FileManager.Git";

GitDBusClient& GitDBusClient::instance()
{
    static GitDBusClient ins;
    return ins;
}

GitDBusClient::GitDBusClient(QObject *parent)
    : QObject(parent)
    , m_interface(nullptr)
    , m_connectionTimer(new QTimer(this))
    , m_serviceAvailable(false)
    , m_signalsConnected(false)
{
    // 设置连接检查定时器
    m_connectionTimer->setInterval(CONNECTION_CHECK_INTERVAL);
    connect(m_connectionTimer, &QTimer::timeout, this, &GitDBusClient::onConnectionCheck);
    
    // 尝试初始连接
    connectToService();
    
    qDebug() << "[GitDBusClient] DBus client initialized";
}

GitDBusClient::~GitDBusClient()
{
    disconnectFromService();
    qDebug() << "[GitDBusClient] DBus client destroyed";
}

bool GitDBusClient::isServiceAvailable() const
{
    return m_serviceAvailable;
}

bool GitDBusClient::connectToService()
{
    if (m_interface) {
        delete m_interface;
        m_interface = nullptr;
    }
    
    // 创建DBus接口
    m_interface = new QDBusInterface(SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, 
                                   QDBusConnection::sessionBus(), this);
    
    if (!m_interface->isValid()) {
        qWarning() << "[GitDBusClient::connectToService] Failed to create DBus interface:" 
                   << m_interface->lastError().message();
        m_serviceAvailable = false;
        m_connectionTimer->start(); // 启动重连定时器
        return false;
    }
    
    // 设置信号连接
    setupSignalConnections();
    
    m_serviceAvailable = true;
    m_connectionTimer->stop(); // 停止重连定时器
    
    qDebug() << "[GitDBusClient::connectToService] Successfully connected to Git service";
    Q_EMIT serviceAvailabilityChanged(true);
    
    return true;
}

void GitDBusClient::disconnectFromService()
{
    cleanupSignalConnections();
    
    if (m_interface) {
        delete m_interface;
        m_interface = nullptr;
    }
    
    if (m_serviceAvailable) {
        m_serviceAvailable = false;
        Q_EMIT serviceAvailabilityChanged(false);
    }
    
    m_connectionTimer->stop();
}

bool GitDBusClient::registerRepository(const QString &repositoryPath)
{
    if (!m_serviceAvailable || !m_interface) {
        qWarning() << "[GitDBusClient::registerRepository] Service not available";
        return false;
    }
    
    QDBusReply<bool> reply = m_interface->call("RegisterRepository", repositoryPath);
    if (!reply.isValid()) {
        handleDBusError("RegisterRepository", reply.error());
        return false;
    }
    
    qDebug() << "[GitDBusClient::registerRepository] Repository registered:" << repositoryPath 
             << "result:" << reply.value();
    return reply.value();
}

bool GitDBusClient::unregisterRepository(const QString &repositoryPath)
{
    if (!m_serviceAvailable || !m_interface) {
        qWarning() << "[GitDBusClient::unregisterRepository] Service not available";
        return false;
    }
    
    QDBusReply<bool> reply = m_interface->call("UnregisterRepository", repositoryPath);
    if (!reply.isValid()) {
        handleDBusError("UnregisterRepository", reply.error());
        return false;
    }
    
    return reply.value();
}

QHash<QString, ItemVersion> GitDBusClient::getFileStatuses(const QStringList &filePaths)
{
    if (!m_serviceAvailable || !m_interface) {
        qWarning() << "[GitDBusClient::getFileStatuses] Service not available";
        return QHash<QString, ItemVersion>();
    }
    
    QDBusReply<QVariantMap> reply = m_interface->call("GetFileStatuses", filePaths);
    if (!reply.isValid()) {
        handleDBusError("GetFileStatuses", reply.error());
        return QHash<QString, ItemVersion>();
    }
    
    return convertFromVariantMap(reply.value());
}

QHash<QString, ItemVersion> GitDBusClient::getRepositoryStatus(const QString &repositoryPath)
{
    if (!m_serviceAvailable || !m_interface) {
        qWarning() << "[GitDBusClient::getRepositoryStatus] Service not available";
        return QHash<QString, ItemVersion>();
    }
    
    QDBusReply<QVariantMap> reply = m_interface->call("GetRepositoryStatus", repositoryPath);
    if (!reply.isValid()) {
        handleDBusError("GetRepositoryStatus", reply.error());
        return QHash<QString, ItemVersion>();
    }
    
    return convertFromVariantMap(reply.value());
}

ItemVersion GitDBusClient::getFileStatus(const QString &filePath)
{
    QStringList paths;
    paths << filePath;
    
    QHash<QString, ItemVersion> result = getFileStatuses(paths);
    return result.value(filePath, UnversionedVersion);
}

bool GitDBusClient::refreshRepository(const QString &repositoryPath)
{
    if (!m_serviceAvailable || !m_interface) {
        qWarning() << "[GitDBusClient::refreshRepository] Service not available";
        return false;
    }
    
    QDBusReply<bool> reply = m_interface->call("RefreshRepository", repositoryPath);
    if (!reply.isValid()) {
        handleDBusError("RefreshRepository", reply.error());
        return false;
    }
    
    return reply.value();
}

bool GitDBusClient::clearRepositoryCache(const QString &repositoryPath)
{
    if (!m_serviceAvailable || !m_interface) {
        qWarning() << "[GitDBusClient::clearRepositoryCache] Service not available";
        return false;
    }
    
    QDBusReply<bool> reply = m_interface->call("ClearRepositoryCache", repositoryPath);
    if (!reply.isValid()) {
        handleDBusError("ClearRepositoryCache", reply.error());
        return false;
    }
    
    return reply.value();
}

QVariantMap GitDBusClient::getServiceStatus()
{
    if (!m_serviceAvailable || !m_interface) {
        qWarning() << "[GitDBusClient::getServiceStatus] Service not available";
        return QVariantMap();
    }
    
    QDBusReply<QVariantMap> reply = m_interface->call("GetServiceStatus");
    if (!reply.isValid()) {
        handleDBusError("GetServiceStatus", reply.error());
        return QVariantMap();
    }
    
    return reply.value();
}

QStringList GitDBusClient::getRegisteredRepositories()
{
    if (!m_serviceAvailable || !m_interface) {
        qWarning() << "[GitDBusClient::getRegisteredRepositories] Service not available";
        return QStringList();
    }
    
    QDBusReply<QStringList> reply = m_interface->call("GetRegisteredRepositories");
    if (!reply.isValid()) {
        handleDBusError("GetRegisteredRepositories", reply.error());
        return QStringList();
    }
    
    return reply.value();
}

void GitDBusClient::requestDirectoryUpdate(const QString &directoryPath)
{
    if (!m_serviceAvailable) {
        return;
    }
    
    // 查找目录下的所有文件
    QDir dir(directoryPath);
    if (!dir.exists()) {
        return;
    }
    
    QStringList filePaths;
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QFileInfo &entry : entries) {
        filePaths.append(entry.absoluteFilePath());
    }
    
    if (!filePaths.isEmpty()) {
        // 异步获取状态
        getFileStatusesAsync(filePaths);
    }
}

void GitDBusClient::getFileStatusesAsync(const QStringList &filePaths)
{
    if (!m_serviceAvailable || !m_interface) {
        return;
    }
    
    // 使用异步调用
    QDBusPendingCall call = m_interface->asyncCall("GetFileStatuses", filePaths);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        QDBusPendingReply<QVariantMap> reply = *watcher;
        if (reply.isValid()) {
            QHash<QString, ItemVersion> statuses = convertFromVariantMap(reply.value());
            Q_EMIT fileStatusesReady(statuses);
        } else {
            handleDBusError("GetFileStatuses (async)", reply.error());
        }
        watcher->deleteLater();
    });
}

void GitDBusClient::getRepositoryStatusAsync(const QString &repositoryPath)
{
    if (!m_serviceAvailable || !m_interface) {
        return;
    }
    
    QDBusPendingCall call = m_interface->asyncCall("GetRepositoryStatus", repositoryPath);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, repositoryPath]() {
        QDBusPendingReply<QVariantMap> reply = *watcher;
        if (reply.isValid()) {
            QHash<QString, ItemVersion> statuses = convertFromVariantMap(reply.value());
            Q_EMIT repositoryStatusReady(repositoryPath, statuses);
        } else {
            handleDBusError("GetRepositoryStatus (async)", reply.error());
        }
        watcher->deleteLater();
    });
}

void GitDBusClient::setupSignalConnections()
{
    if (!m_interface || m_signalsConnected) {
        return;
    }
    
    // 连接DBus信号
    bool success = true;
    success &= QDBusConnection::sessionBus().connect(SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME,
                                                   "RepositoryStatusChanged", this,
                                                   SLOT(onRepositoryStatusChanged(QString, QVariantMap)));
    
    success &= QDBusConnection::sessionBus().connect(SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME,
                                                   "RepositoryDiscovered", this,
                                                   SLOT(onRepositoryDiscovered(QString)));
    
    // 监控服务的生命周期
    success &= QDBusConnection::sessionBus().connect("", "", "org.freedesktop.DBus",
                                                   "NameOwnerChanged", this,
                                                   SLOT(onServiceOwnerChanged(QString, QString, QString)));
    
    if (success) {
        m_signalsConnected = true;
        qDebug() << "[GitDBusClient::setupSignalConnections] Signal connections established";
    } else {
        qWarning() << "[GitDBusClient::setupSignalConnections] Failed to connect to some signals";
    }
}

void GitDBusClient::cleanupSignalConnections()
{
    if (!m_signalsConnected) {
        return;
    }
    
    QDBusConnection::sessionBus().disconnect(SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME,
                                           "RepositoryStatusChanged", this,
                                           SLOT(onRepositoryStatusChanged(QString, QVariantMap)));
    
    QDBusConnection::sessionBus().disconnect(SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME,
                                           "RepositoryDiscovered", this,
                                           SLOT(onRepositoryDiscovered(QString)));
    
    QDBusConnection::sessionBus().disconnect("", "", "org.freedesktop.DBus",
                                           "NameOwnerChanged", this,
                                           SLOT(onServiceOwnerChanged(QString, QString, QString)));
    
    m_signalsConnected = false;
}

QHash<QString, ItemVersion> GitDBusClient::convertFromVariantMap(const QVariantMap &variantMap)
{
    QHash<QString, ItemVersion> result;
    
    for (auto it = variantMap.begin(); it != variantMap.end(); ++it) {
        ItemVersion status = static_cast<ItemVersion>(it.value().toInt());
        result.insert(it.key(), status);
    }
    
    return result;
}

QVariantMap GitDBusClient::convertToVariantMap(const QHash<QString, ItemVersion> &statusMap)
{
    QVariantMap result;
    
    for (auto it = statusMap.begin(); it != statusMap.end(); ++it) {
        result.insert(it.key(), static_cast<int>(it.value()));
    }
    
    return result;
}

void GitDBusClient::handleDBusError(const QString &method, const QDBusError &error)
{
    qWarning() << "[GitDBusClient::" << method << "] DBus error:" << error.message();
    
    // 如果是连接错误，尝试重连
    if (error.type() == QDBusError::Disconnected || 
        error.type() == QDBusError::ServiceUnknown) {
        m_serviceAvailable = false;
        Q_EMIT serviceAvailabilityChanged(false);
        m_connectionTimer->start(); // 启动重连
    }
}

void GitDBusClient::onRepositoryStatusChanged(const QString &repositoryPath, const QVariantMap &changes)
{
    qDebug() << "[GitDBusClient::onRepositoryStatusChanged] Repository:" << repositoryPath 
             << "changes:" << changes.size();
    Q_EMIT repositoryStatusChanged(repositoryPath, changes);
}

void GitDBusClient::onRepositoryDiscovered(const QString &repositoryPath)
{
    qDebug() << "[GitDBusClient::onRepositoryDiscovered] Repository discovered:" << repositoryPath;
    Q_EMIT repositoryDiscovered(repositoryPath);
}

void GitDBusClient::onServiceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner)
{
    if (serviceName == SERVICE_NAME) {
        bool wasAvailable = !oldOwner.isEmpty();
        bool isAvailable = !newOwner.isEmpty();
        
        if (wasAvailable != isAvailable) {
            qDebug() << "[GitDBusClient::onServiceOwnerChanged] Service" << serviceName 
                     << "availability changed:" << isAvailable;
            
            if (isAvailable) {
                // 服务重新可用，尝试重连
                connectToService();
            } else {
                // 服务不可用
                m_serviceAvailable = false;
                Q_EMIT serviceAvailabilityChanged(false);
                m_connectionTimer->start();
            }
        }
    }
}

void GitDBusClient::onConnectionCheck()
{
    if (!m_serviceAvailable) {
        qDebug() << "[GitDBusClient::onConnectionCheck] Attempting to reconnect to service";
        connectToService();
    }
} 