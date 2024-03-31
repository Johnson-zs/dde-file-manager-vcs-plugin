#ifndef GITWINDOWPLUGIN_H
#define GITWINDOWPLUGIN_H

#include <dfm-extension/window/dfmextwindowplugin.h>

#include <QString>

#include "gitrepowatcher.h"

class GitWindowPlugin : public DFMEXT::DFMExtWindowPlugin
{
public:
    GitWindowPlugin();
    void windowUrlChanged(std::uint64_t winId,
                          const std::string &urlString) DFM_FAKE_OVERRIDE;
    void windowClosed(std::uint64_t winId) DFM_FAKE_OVERRIDE;

private:
    bool retrieval(const QString &directory);

private:
    GitRepoWatcher m_repoWatcher;
};

#endif   // GITWINDOWPLUGIN_H
