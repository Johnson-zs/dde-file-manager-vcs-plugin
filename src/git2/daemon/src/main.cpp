#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
#include "git-daemon.h"
#include "git-service.h"
#include "gitservice_adaptor.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // 注册DBus服务
    auto connection = QDBusConnection::sessionBus();
    if (!connection.registerService("org.deepin.FileManager.Git")) {
        qWarning() << "Failed to register DBus service:" << connection.lastError().message();
        return 1;
    }
    
    // 创建守护进程管理器
    GitDaemon daemon;
    if (!daemon.initialize()) {
        qCritical() << "Failed to initialize Git daemon";
        return 1;
    }
    
    // 创建自动生成的适配器
    new GitAdaptor(daemon.service());
    
    // 注册DBus对象
    if (!connection.registerObject("/org/deepin/filemanager/git", daemon.service())) {
        qWarning() << "Failed to register DBus object:" << connection.lastError().message();
        return 1;
    }
    
    qDebug() << "DDE File Manager Git Daemon started successfully";
    
    return app.exec();
} 