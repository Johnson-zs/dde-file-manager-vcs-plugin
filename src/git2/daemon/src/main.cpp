#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
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
    
    // 创建服务对象
    GitService gitService;
    
    // 创建自动生成的适配器
    new GitAdaptor(&gitService);
    
    // 注册DBus对象
    if (!connection.registerObject("/org/deepin/filemanager/git", &gitService)) {
        qWarning() << "Failed to register DBus object:" << connection.lastError().message();
        return 1;
    }
    
    qDebug() << "DDE File Manager Git Daemon started successfully";
    
    return app.exec();
} 