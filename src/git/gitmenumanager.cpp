#include "gitmenumanager.h"
#include "gitoperationservice.h"
#include "gitmenubuilder.h"
#include "utils.h"

#include <QFileInfo>
#include <algorithm>

GitMenuManager::GitMenuManager(QObject *parent)
    : QObject(parent), m_proxy(nullptr), m_operationService(new GitOperationService(this)), m_menuBuilder(nullptr)
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

    // 检查是否在Git仓库中或者是仓库根目录本身
    if (!Utils::isInsideRepositoryFile(filePath) && !Utils::isGitRepositoryRoot(filePath)) {
        return false;
    }

    // 创建Git根菜单
    auto rootAction = m_proxy->createAction();
    rootAction->setText("Git...");

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

        auto actions = main->actions();
        auto it = std::find_if(actions.cbegin(), actions.cend(), [](const auto *action) {
            const std::string &text = action->text();
            return (text.find("打开方式") == 0) || (text.find("Open with") == 0);
        });

        if (it != actions.cend()) {
            // 在"打开方式"的下一个菜单项之前插入Git菜单
            main->insertAction(*it, rootAction);
        } else {
            // 如果没有找到"打开方式"，直接添加到末尾
            main->addAction(rootAction);
        }

        qInfo() << "INFO: [GitMenuManager::buildNormalMenu] Git menu added to normal menu";
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

    // 查找"显示方式"菜单项，在其上方插入Git菜单
    auto actions = main->actions();
    auto it = std::find_if(actions.cbegin(), actions.cend(), [](const auto *action) {
        const std::string &text = action->text();
        return (text.find("以管理员身份打开") == 0) || (text.find("Open as administrator") == 0);
    });

    // 直接在主菜单中添加Git菜单项，而不是创建子菜单
    bool hasValidAction = false;

    if (it != actions.end()) {
        // 在"以管理员身份打开"之前插入Git菜单项
        hasValidAction = m_menuBuilder->buildRepositoryMenuItems(main, repositoryPath, *it);
    } else {
        // 如果没有找到"以管理员身份打开"，直接添加到末尾
        hasValidAction = m_menuBuilder->buildRepositoryMenuItems(main, repositoryPath);
    }

    if (hasValidAction) {
        qInfo() << "INFO: [GitMenuManager::buildEmptyAreaMenu] Git menu items added to empty area menu";
    }

    return hasValidAction;
}

void GitMenuManager::onGitOperationCompleted(const QString &operation, bool success)
{
    if (success) {
        qInfo() << "INFO: [GitMenuManager::onGitOperationCompleted] Operation completed successfully:" << operation;
    } else {
        qWarning() << "WARNING: [GitMenuManager::onGitOperationCompleted] Operation failed:" << operation;
    }
}
