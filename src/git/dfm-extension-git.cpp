#include <dfm-extension/dfm-extension.h>

#include <cache.h>

#include "gitemblemiconplugin.h"
#include "gitmenuplugin.h"
#include "gitwindowplugin.h"

#include <QCoreApplication>

static DFMEXT::DFMExtMenuPlugin *gitMenu { nullptr };
static DFMEXT::DFMExtEmblemIconPlugin *gitEmblemIcon { nullptr };
static DFMEXT::DFMExtWindowPlugin *gitWindowPlugin { nullptr };

extern "C" {
void dfm_extension_initiliaze()
{
    if (qApp->applicationName() == "dde-file-manager") {
        gitMenu = new GitMenuPlugin;
        gitEmblemIcon = new GitEmblemIconPlugin;
        gitWindowPlugin = new GitWindowPlugin;
    }
}

void dfm_extension_shutdown()
{
    delete gitMenu;
    delete gitEmblemIcon;
    delete gitWindowPlugin;
}

DFMEXT::DFMExtMenuPlugin *dfm_extension_menu()
{
    return gitMenu;
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
