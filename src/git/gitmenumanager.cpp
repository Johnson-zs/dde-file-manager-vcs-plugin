#include "gitmenumanager.h"
#include "gitoperationservice.h"
#include "gitmenubuilder.h"
#include "utils.h"

#include <QFileInfo>

GitMenuManager::GitMenuManager(QObject *parent)
    : QObject(parent)
    , m_proxy(nullptr)
    , m_operationService(new GitOperationService(this))
    , m_menuBuilder(nullptr)
{
    // 连接操作服务的信号
    connect(m_operationService, &GitOperationService::operationCompleted,
            this, &GitMenuManager::onGitOperationCompleted);
}

GitMenuManager::~GitMenuManager() = default;

void GitMenuManager::initialize(DFMEXT::DFMExtMenuProxy *proxy)
{
    m_proxy = proxy;
    
    // 创建菜单构建器（需要proxy和operationService）
    m_menuBuilder = new GitMenuBuilder(proxy, m_operationService, this);
    
    qInfo() << "INFO: [GitMenuManager::initialize] Git menu manager initialized successfully";
}

bool GitMenuManager::buildNormalMenu(DFMEXT::DFMExtMenu *main,
                                     const std::string &currentPath,
                                     const std::string &focusPath,
                                     const std::list<std::string> &pathList,
                                     bool onDesktop)
{
    if (onDesktop || !m_menuBuilder) {
        return false;
    }

    const QString filePath = QString::fromStdString(focusPath);

    // 检查是否在Git仓库中
    if (!Utils::isInsideRepositoryFile(filePath)) {
        return false;
    }

    // 创建Git根菜单
    auto rootAction = m_proxy->createAction();
    rootAction->setText("Git");

    auto gitSubmenu = m_proxy->createMenu();
    rootAction->setMenu(gitSubmenu);

    bool hasValidAction = false;

    // 处理多文件选择
    if (pathList.size() > 1) {
        hasValidAction = m_menuBuilder->buildMultiFileMenu(gitSubmenu, pathList);
    } else {
        // 单文件菜单处理
        const QString currentPathStr = QString::fromStdString(currentPath);
        hasValidAction = m_menuBuilder->buildSingleFileMenu(gitSubmenu, currentPathStr, filePath);
    }

    if (hasValidAction) {
        main->addAction(rootAction);
        return true;
    }

    return false;
}

bool GitMenuManager::buildEmptyAreaMenu(DFMEXT::DFMExtMenu *main,
                                       const std::string &currentPath,
                                       bool onDesktop)
{
    if (onDesktop || !m_menuBuilder) {
        return false;
    }

    const QString dirPath = QString::fromStdString(currentPath);
    if (!Utils::isInsideRepositoryDir(dirPath)) {
        return false;
    }

    const QString repositoryPath = Utils::repositoryBaseDir(dirPath);
    if (repositoryPath.isEmpty()) {
        return false;
    }

    return m_menuBuilder->buildRepositoryMenu(main, repositoryPath);
}

void GitMenuManager::onGitOperationCompleted(const QString &operation, bool success)
{
    if (success) {
        qInfo() << "INFO: [GitMenuManager::onGitOperationCompleted] Operation completed successfully:" << operation;
    } else {
        qWarning() << "WARNING: [GitMenuManager::onGitOperationCompleted] Operation failed:" << operation;
    }
} 