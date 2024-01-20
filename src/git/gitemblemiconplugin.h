#ifndef GITEMBLEMICONPLUGIN_H
#define GITEMBLEMICONPLUGIN_H

#include <dfm-extension/emblemicon/dfmextemblemiconplugin.h>

class GitEmblemIconPlugin : public DFMEXT::DFMExtEmblemIconPlugin
{
public:
    GitEmblemIconPlugin();
    DFMEXT::DFMExtEmblem locationEmblemIcons(const std::string &filePath, int systemIconCount) const DFM_FAKE_OVERRIDE;
};

#endif   // GITEMBLEMICONPLUGIN_H
