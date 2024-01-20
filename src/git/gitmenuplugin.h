#ifndef GITMENUPLUGIN_H
#define GITMENUPLUGIN_H

#include <dfm-extension/menu/dfmextmenuplugin.h>

class GitMenuPlugin : public DFMEXT::DFMExtMenuPlugin
{
public:
    GitMenuPlugin();

    void initialize(DFMEXT::DFMExtMenuProxy *proxy) DFM_FAKE_OVERRIDE;
    bool buildNormalMenu(DFMEXT::DFMExtMenu *main,
                         const std::string &currentPath,
                         const std::string &focusPath,
                         const std::list<std::string> &pathList,
                         bool onDesktop) DFM_FAKE_OVERRIDE;
    bool buildEmptyAreaMenu(DFMEXT::DFMExtMenu *main, const std::string &currentPath, bool onDesktop) DFM_FAKE_OVERRIDE;

private:
    DFMEXT::DFMExtMenuProxy *m_proxy { nullptr };
};

#endif   // GITMENUPLUGIN_H
