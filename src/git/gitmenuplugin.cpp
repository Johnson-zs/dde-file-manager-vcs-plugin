#include "gitmenuplugin.h"

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>
#include <dfm-extension/menu/dfmextaction.h>

USING_DFMEXT_NAMESPACE

GitMenuPlugin::GitMenuPlugin()
{
}

void GitMenuPlugin::initialize(dfmext::DFMExtMenuProxy *proxy)
{
    m_proxy = proxy;
}

bool GitMenuPlugin::buildNormalMenu(dfmext::DFMExtMenu *main, const std::string &currentPath, const std::string &focusPath, const std::list<std::string> &pathList, bool onDesktop)
{
    return true;
}

bool GitMenuPlugin::buildEmptyAreaMenu(dfmext::DFMExtMenu *main, const std::string &currentPath, bool onDesktop)
{
    return true;
}
