#ifndef GITMENUMANAGER_H
#define GITMENUMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <list>
#include <string>

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>
#include <dfm-extension/menu/dfmextaction.h>

class GitOperationService;
class GitMenuBuilder;

/**
 * @brief Git菜单管理器
 * 
 * 作为QObject子类，负责协调菜单构建和Git操作
 * 使用组合模式将职责分离到专门的服务类
 */
class GitMenuManager : public QObject
{
    Q_OBJECT

public:
    explicit GitMenuManager(QObject *parent = nullptr);
    ~GitMenuManager();

    /**
     * @brief 初始化菜单管理器
     * @param proxy DFM扩展菜单代理
     */
    void initialize(DFMEXT::DFMExtMenuProxy *proxy);

    /**
     * @brief 构建普通文件菜单
     */
    bool buildNormalMenu(DFMEXT::DFMExtMenu *main,
                         const std::string &currentPath,
                         const std::string &focusPath,
                         const std::list<std::string> &pathList,
                         bool onDesktop);

    /**
     * @brief 构建空白区域菜单
     */
    bool buildEmptyAreaMenu(DFMEXT::DFMExtMenu *main,
                           const std::string &currentPath,
                           bool onDesktop);

private Q_SLOTS:
    /**
     * @brief Git操作完成后的处理
     */
    void onGitOperationCompleted(const QString &operation, bool success);

private:
    DFMEXT::DFMExtMenuProxy *m_proxy;
    GitOperationService *m_operationService;
    GitMenuBuilder *m_menuBuilder;
};

#endif // GITMENUMANAGER_H 