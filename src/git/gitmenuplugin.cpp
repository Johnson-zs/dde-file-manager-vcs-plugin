#include "gitmenuplugin.h"
#include "utils.h"

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>
#include <dfm-extension/menu/dfmextaction.h>
#include <dfm-extension/window/dfmextwindowplugin.h>

#include <QObject>

USING_DFMEXT_NAMESPACE

GitMenuPlugin::GitMenuPlugin()
    : DFMEXT::DFMExtMenuPlugin()
{
    registerInitialize([this](DFMEXT::DFMExtMenuProxy *proxy) {
        initialize(proxy);
    });
    registerBuildNormalMenu([this](DFMExtMenu *main, const std::string &currentPath,
                                   const std::string &focusPath, const std::list<std::string> &pathList,
                                   bool onDesktop) {
        return buildNormalMenu(main, currentPath, focusPath, pathList, onDesktop);
    });
    registerBuildEmptyAreaMenu([this](DFMExtMenu *main, const std::string &currentPath, bool onDesktop) {
        return buildEmptyAreaMenu(main, currentPath, onDesktop);
    });
}

void GitMenuPlugin::initialize(dfmext::DFMExtMenuProxy *proxy)
{
    m_proxy = proxy;
}

bool GitMenuPlugin::buildNormalMenu(dfmext::DFMExtMenu *main, const std::string &currentPath,
                                    const std::string &focusPath, const std::list<std::string> &pathList, bool onDesktop)
{
    if (onDesktop)
        return false;
    return true;
}

bool GitMenuPlugin::buildEmptyAreaMenu(dfmext::DFMExtMenu *main, const std::string &currentPath, bool onDesktop)
{
    if (onDesktop)
        return false;
    if (!Utils::isInsideRepositoryDir(QString::fromStdString(currentPath)))
        return false;

    auto checkoutAct { m_proxy->createAction() };
    auto logAct { m_proxy->createAction() };
    auto pushAct { m_proxy->createAction() };
    auto pullAct { m_proxy->createAction() };

    // TODO: check git repositor state

    checkoutAct->setText("Git Checkout...");
    logAct->setText("Git Log...");
    pushAct->setText("Git Push...");
    pullAct->setText("Git Pull...");

    main->addAction(checkoutAct);
    main->addAction(logAct);
    main->addAction(pushAct);
    main->addAction(pullAct);

    return true;
}
