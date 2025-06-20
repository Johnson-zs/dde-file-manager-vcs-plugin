#include <dfm-extension/dfm-extension.h>
#include "git-emblem-plugin.h"
#include <QCoreApplication>
#include <QDebug>

static DFMEXT::DFMExtEmblemIconPlugin *gitEmblemIcon = nullptr;

extern "C" {

void dfm_extension_initiliaze()
{
    if (qApp->applicationName() == "dde-file-manager") {
        gitEmblemIcon = new GitEmblemPlugin;
        qDebug() << "[Git Extension] Plugin initialized successfully";
    }
}

void dfm_extension_shutdown()
{
    delete gitEmblemIcon;
    gitEmblemIcon = nullptr;
    qDebug() << "[Git Extension] Plugin shutdown";
}

DFMEXT::DFMExtEmblemIconPlugin *dfm_extension_emblem()
{
    return gitEmblemIcon;
}

}   // extern "C" 