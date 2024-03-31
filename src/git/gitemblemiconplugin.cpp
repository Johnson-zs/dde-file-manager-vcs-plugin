#include "gitemblemiconplugin.h"

#include <filesystem>

#include <QString>

#include <cache.h>
#include "utils.h"

USING_DFMEXT_NAMESPACE

GitEmblemIconPlugin::GitEmblemIconPlugin()
    : DFMEXT::DFMExtEmblemIconPlugin()
{
    registerLocationEmblemIcons([this](const std::string &filePath, int systemIconCount) {
        return locationEmblemIcons(filePath, systemIconCount);
    });
}

// work in thread
DFMExtEmblem GitEmblemIconPlugin::locationEmblemIcons(const std::string &filePath, int systemIconCount) const
{
    using Global::ItemVersion;
    const QString &path { QString::fromStdString(filePath) };
    DFMExtEmblem emblem;
    if (!Utils::isInsideRepositoryFile(path))
        return emblem;

    auto state { Global::Cache::instance().version(path) };
    std::vector<DFMExtEmblemIconLayout> layouts;
    QString iconName;

    switch (state) {
    case ItemVersion::NormalVersion:
        iconName = QStringLiteral("vcs-normal");
        break;
    case ItemVersion::UpdateRequiredVersion:
        iconName = QStringLiteral("vcs-update-required");
        break;
    case ItemVersion::LocallyModifiedVersion:
        iconName = QStringLiteral("vcs-locally-modified");
        break;
    case ItemVersion::LocallyModifiedUnstagedVersion:
        iconName = QStringLiteral("vcs-locally-modified-unstaged");
        break;
    case ItemVersion::AddedVersion:
        iconName = QStringLiteral("vcs-added");
        break;
    case ItemVersion::RemovedVersion:
        iconName = QStringLiteral("vcs-removed");
        break;
    case ItemVersion::ConflictingVersion:
        iconName = QStringLiteral("vcs-conflicting");
        break;
    case ItemVersion::UnversionedVersion:
    case ItemVersion::IgnoredVersion:
    case ItemVersion::MissingVersion:
        break;
    default:
        Q_ASSERT(false);
        break;
    }

    if (Utils::isDirectoryEmpty(path))
        iconName.clear();

    DFMExtEmblemIconLayout iconLayout { DFMExtEmblemIconLayout::LocationType::BottomLeft, iconName.toStdString() };
    layouts.push_back(iconLayout);
    emblem.setEmblem(layouts);

    return emblem;
}
