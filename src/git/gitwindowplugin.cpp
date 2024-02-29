#include "gitwindowplugin.h"
#include "utils.h"

#include <QUrl>
#include <QString>

GitWindowPlugin::GitWindowPlugin()
{
    registerWindowUrlChanged([this](std::uint64_t winId, const std::string &urlString) {
        return windowUrlChanged(winId, urlString);
    });
    registerWindowClosed([this](std::uint64_t winId) {
        lastWindowClosed(winId);
    });
}

void GitWindowPlugin::windowUrlChanged(std::uint64_t winId, const std::string &urlString)
{
    QUrl url { QString::fromStdString(urlString) };
    if (!url.isValid() || !url.isLocalFile())
        return;

    if (!Utils::isInsideRepositoryDir(url.toLocalFile()))
        return;

    QString rootPath { Utils::repositoryBaseDir(url.toLocalFile()) };

    // Window(1) -> Repository(1) -> Status(n)
}

void GitWindowPlugin::lastWindowClosed(std::uint64_t winId)
{
}
