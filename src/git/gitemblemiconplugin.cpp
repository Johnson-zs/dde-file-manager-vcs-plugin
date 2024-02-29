#include "gitemblemiconplugin.h"
#include "utils.h"

#include <QString>

#include <filesystem>

USING_DFMEXT_NAMESPACE

GitEmblemIconPlugin::GitEmblemIconPlugin()
    : DFMEXT::DFMExtEmblemIconPlugin()
{
    registerLocationEmblemIcons([this](const std::string &filePath, int systemIconCount) {
        return locationEmblemIcons(filePath, systemIconCount);
    });
}

DFMExtEmblem GitEmblemIconPlugin::locationEmblemIcons(const std::string &filePath, int systemIconCount) const
{
    DFMExtEmblem emblem;
    if (!Utils::isInsideRepositoryFile(QString::fromStdString(filePath)))
        return emblem;

    // QString dirPath;
    // if (std::filesystem::is_directory(filePath))
    //     dirPath = QString::fromStdString(filePath);
    // else
    //     dirPath = QString::fromStdString(std::filesystem::path { filePath }.parent_path().string());

    // Q_ASSERT(!dirPath.isEmpty());
    // // TODO: performance, use cache
    // if (!Utils::isInsideRepositoryDir(dirPath))
    //     return emblem;

    // // TODO:  git --no-optional-locks status --porcelain -z -u --ignored
    // std::vector<DFMExtEmblemIconLayout> layouts;
    // DFMExtEmblemIconLayout iconLayout { DFMExtEmblemIconLayout::LocationType::BottomLeft, "vcs-normal" };
    // layouts.push_back(iconLayout);
    // emblem.setEmblem(layouts);

    return emblem;
}
