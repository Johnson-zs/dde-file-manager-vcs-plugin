#include "git-window-plugin.h"
#include "git-dbus-client.h"
#include "git-version-controller.h"
#include <QDebug>
#include <QUrl>

GitWindowPlugin::GitWindowPlugin()
    : DFMEXT::DFMExtWindowPlugin()
    , m_initialized(false)
    , m_controller(nullptr)
{
    // 注册回调函数
    registerWindowOpened([this](std::uint64_t winId) {
        windowOpened(winId);
    });
    
    registerWindowClosed([this](std::uint64_t winId) {
        windowClosed(winId);
    });
    
    registerFirstWindowOpened([this](std::uint64_t winId) {
        firstWindowOpened(winId);
    });
    
    registerLastWindowClosed([this](std::uint64_t winId) {
        lastWindowClosed(winId);
    });
    
    registerWindowUrlChanged([this](std::uint64_t winId, const std::string &urlString) {
        windowUrlChanged(winId, urlString);
    });
    
    qDebug() << "[GitWindowPlugin] Plugin initialized";
}

GitWindowPlugin::~GitWindowPlugin()
{
    qDebug() << "[GitWindowPlugin] Plugin destroyed";
}

void GitWindowPlugin::windowOpened(std::uint64_t winId)
{
    qDebug() << "[GitWindowPlugin::windowOpened] Window opened, ID:" << winId;
}

void GitWindowPlugin::windowClosed(std::uint64_t winId)
{
    qDebug() << "[GitWindowPlugin::windowClosed] Window closed, ID:" << winId;
}

void GitWindowPlugin::firstWindowOpened(std::uint64_t winId)
{
    qDebug() << "[GitWindowPlugin::firstWindowOpened] First window opened, ID:" << winId;
    
    if (!m_initialized) {
        // 初始化DBus客户端连接
        if (GitDBusClient::instance().connectToService()) {
            qDebug() << "[GitWindowPlugin::firstWindowOpened] Connected to Git daemon service";
        } else {
            qWarning() << "[GitWindowPlugin::firstWindowOpened] Failed to connect to Git daemon service";
        }
        
        // 创建版本控制器
        if (!m_controller) {
            m_controller.reset(new GitVersionController);
            qDebug() << "[GitWindowPlugin::firstWindowOpened] GitVersionController created";
        }
        
        m_initialized = true;
    }
}

void GitWindowPlugin::lastWindowClosed(std::uint64_t winId)
{
    qDebug() << "[GitWindowPlugin::lastWindowClosed] Last window closed, ID:" << winId;
    handleLastWindowClosed();
}

void GitWindowPlugin::windowUrlChanged(std::uint64_t winId, const std::string &urlString)
{
    const QUrl url = QUrl(QString::fromStdString(urlString));
    
    // 只处理本地文件URL
    if (!url.isValid() || !url.isLocalFile()) {
        return;
    }
    
    const QString localPath = url.toLocalFile();
    qDebug() << "[GitWindowPlugin::windowUrlChanged] Window" << winId << "URL changed to:" << localPath;
    
    // 注册新的目录到Git服务进行仓库发现
    GitDBusClient::instance().registerRepository(localPath);
    
    // 如果有版本控制器，触发状态检索
    if (m_controller) {
        Q_EMIT m_controller->requestRetrieval(url);
        qDebug() << "[GitWindowPlugin::windowUrlChanged] Triggered retrieval for:" << localPath;
    }
}

void GitWindowPlugin::handleLastWindowClosed()
{
    qDebug() << "[GitWindowPlugin::handleLastWindowClosed] Handling last window closed event";
    
    // 调用daemon服务清理所有资源
    if (GitDBusClient::instance().isServiceAvailable()) {
        bool success = GitDBusClient::instance().clearAllResources();
        if (success) {
            qDebug() << "[GitWindowPlugin::handleLastWindowClosed] Successfully cleared all daemon resources";
        } else {
            qWarning() << "[GitWindowPlugin::handleLastWindowClosed] Failed to clear daemon resources";
        }
    } else {
        qWarning() << "[GitWindowPlugin::handleLastWindowClosed] Git daemon service not available";
    }
    
    // 清理版本控制器
    m_controller.reset();
    
    // 断开DBus连接
    GitDBusClient::instance().disconnectFromService();
    m_initialized = false;
    
    qDebug() << "[GitWindowPlugin::handleLastWindowClosed] Resource cleanup completed";
}
