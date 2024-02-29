#ifndef GITWINDOWPLUGIN_H
#define GITWINDOWPLUGIN_H

#include <dfm-extension/window/dfmextwindowplugin.h>

class GitWindowPlugin : public DFMEXT::DFMExtWindowPlugin
{
public:
    GitWindowPlugin();
    void windowUrlChanged(std::uint64_t winId,
                          const std::string &urlString) DFM_FAKE_OVERRIDE;
    void lastWindowClosed(std::uint64_t winId) DFM_FAKE_OVERRIDE;
};

#endif   // GITWINDOWPLUGIN_H
