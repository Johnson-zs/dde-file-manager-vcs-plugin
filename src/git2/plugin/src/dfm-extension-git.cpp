#include <dfm-extension/dfm-extension.h>
#include "git-emblem-plugin.h"
#include "git-window-plugin.h"
#include <QCoreApplication>
#include <QDebug>

static DFMEXT::DFMExtEmblemIconPlugin *gitEmblemIcon = nullptr;
static DFMEXT::DFMExtWindowPlugin *gitWindowPlugin = nullptr;

extern "C" {

void dfm_extension_initiliaze()
{
    if (qApp->applicationName() == "dde-file-manager") {
        gitEmblemIcon = new GitEmblemPlugin;
        gitWindowPlugin = new GitWindowPlugin;
        qDebug() << "[Git Extension] Plugin initialized successfully";
    }
}

void dfm_extension_shutdown()
{
    delete gitEmblemIcon;
    gitEmblemIcon = nullptr;
    delete gitWindowPlugin;
    gitWindowPlugin = nullptr;
    qDebug() << "[Git Extension] Plugin shutdown";
}

DFMEXT::DFMExtEmblemIconPlugin *dfm_extension_emblem()
{
    return gitEmblemIcon;
}

DFMEXT::DFMExtWindowPlugin *dfm_extension_window()
{
    return gitWindowPlugin;
}

}   // extern "C" 