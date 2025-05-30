#ifndef GITMENUPLUGIN_H
#define GITMENUPLUGIN_H

#include <dfm-extension/menu/dfmextmenuplugin.h>
#include <dfm-extension/menu/dfmextaction.h>

#include <QString>
#include <QScopedPointer>

class GitMenuManager;

/**
 * @brief Git菜单插件入口类
 * 
 * 轻量级插件入口，负责插件生命周期管理和接口适配
 * 实际业务逻辑委托给GitMenuManager处理
 */
class GitMenuPlugin : public DFMEXT::DFMExtMenuPlugin
{
public:
    GitMenuPlugin();
    ~GitMenuPlugin();

    void initialize(DFMEXT::DFMExtMenuProxy *proxy) DFM_FAKE_OVERRIDE;
    bool buildNormalMenu(DFMEXT::DFMExtMenu *main,
                         const std::string &currentPath,
                         const std::string &focusPath,
                         const std::list<std::string> &pathList,
                         bool onDesktop) DFM_FAKE_OVERRIDE;
    bool buildEmptyAreaMenu(DFMEXT::DFMExtMenu *main, 
                           const std::string &currentPath, 
                           bool onDesktop) DFM_FAKE_OVERRIDE;

private:
    QScopedPointer<GitMenuManager> m_menuManager;
};

#endif // GITMENUPLUGIN_H
