#include "gitmenuplugin.h"
#include "gitmenumanager.h"

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>
#include <dfm-extension/menu/dfmextaction.h>

USING_DFMEXT_NAMESPACE

GitMenuPlugin::GitMenuPlugin()
    : DFMEXT::DFMExtMenuPlugin()
    , m_menuManager(new GitMenuManager())
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

GitMenuPlugin::~GitMenuPlugin() = default;

void GitMenuPlugin::initialize(dfmext::DFMExtMenuProxy *proxy)
{
    m_menuManager->initialize(proxy);
}

bool GitMenuPlugin::buildNormalMenu(dfmext::DFMExtMenu *main, const std::string &currentPath,
                                    const std::string &focusPath, const std::list<std::string> &pathList, bool onDesktop)
{
    return m_menuManager->buildNormalMenu(main, currentPath, focusPath, pathList, onDesktop);
}

bool GitMenuPlugin::buildEmptyAreaMenu(dfmext::DFMExtMenu *main, const std::string &currentPath, bool onDesktop)
{
    return m_menuManager->buildEmptyAreaMenu(main, currentPath, onDesktop);
}
