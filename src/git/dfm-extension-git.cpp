#include <dfm-extension/dfm-extension.h>

#include "gitemblemiconplugin.h"
#include "gitmenuplugin.h"

static DFMEXT::DFMExtMenuPlugin *gitMenu { nullptr };
static DFMEXT::DFMExtEmblemIconPlugin *gitEmblemIcon { nullptr };

extern "C" {
void dfm_extension_initiliaze()
{
    gitMenu = new GitMenuPlugin;
    gitEmblemIcon = new GitEmblemIconPlugin;
}

void dfm_extension_shutdown()
{
    delete gitMenu;
    delete gitEmblemIcon;
}

DFMEXT::DFMExtMenuPlugin *dfm_extension_menu()
{
    return gitMenu;
}

DFMEXT::DFMExtEmblemIconPlugin *dfm_extension_emblem()
{
    return gitEmblemIcon;
}
}   // extern "C"
