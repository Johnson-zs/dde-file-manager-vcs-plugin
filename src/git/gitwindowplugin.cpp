#include "gitwindowplugin.h"

#include <QUrl>
#include <QProcess>
#include <QTextCodec>
#include <QFileInfo>

#include <cache.h>

#include "utils.h"

using Global::ItemVersion;

void GitVersionWorker::retrieval(const QUrl &url)
{
    if (!Utils::isInsideRepositoryDir(url.toLocalFile()))
        return;

    const QString &directory { url.toLocalFile() + "/" };
    const QString &repositoryPath { Utils::repositoryBaseDir(directory) };
    if (Q_UNLIKELY(repositoryPath.isEmpty()))
        return;

    // clear version
    Global::Cache::instance().removeVersion(repositoryPath);

    // cache git status for current path
    QProcess process;
    process.setWorkingDirectory(repositoryPath);
    process.start("git", { "--no-optional-locks", "status", "--porcelain", "-z", "-u", "--ignored" });
    const QString &dirBelowBaseDir { Utils::findPathBelowGitBaseDir(directory) };
    QHash<QString, ItemVersion> versionInfoHash;
    while (process.waitForReadyRead()) {
        char buffer[1024];
        while (Utils::readUntilZeroChar(&process, buffer, sizeof(buffer)) > 0) {
            const QString &line { QTextCodec::codecForLocale()->toUnicode(buffer) };
            // X and Y from the table in `man git-status`
            const auto [X, Y, fileName] { Utils::parseLineGitStatus(line) };
            ItemVersion state { ItemVersion::NormalVersion };
            // Renames list the old file name directly afterwards, separated by \0.
            if (X == 'R') {
                state = ItemVersion::LocallyModifiedVersion;
                Utils::readUntilZeroChar(&process, nullptr, 0);   // discard old file name
            }
            state = Utils::parseXYState(state, X, Y);
            // decide what to record about that file
            if (state == ItemVersion::NormalVersion || !fileName.startsWith(dirBelowBaseDir))
                continue;

            // File name relative to the current working directory.
            const QString &relativeFileName { fileName.mid(dirBelowBaseDir.length()) };
            const QString &absoluteFileName { directory + relativeFileName };
            Q_ASSERT(QUrl::fromLocalFile(absoluteFileName).isValid());
            // normal file, no directory
            versionInfoHash.insert(absoluteFileName, state);

            // if file is part of a sub-directory, record the directory
            if (relativeFileName.contains('/')) {
                if (state == ItemVersion::IgnoredVersion)
                    continue;
                if (state == ItemVersion::AddedVersion || state == ItemVersion::RemovedVersion)
                    state = ItemVersion::LocallyModifiedVersion;
                const QStringList &absoluteDirNames { Utils::makeDirGroup(directory, relativeFileName) };
                for (const auto &absoluteDirName : absoluteDirNames) {
                    Q_ASSERT(QUrl::fromLocalFile(absoluteDirName).isValid());
                    if (versionInfoHash.contains(absoluteDirName)) {
                        ItemVersion oldState = versionInfoHash.value(absoluteDirName);
                        // only keep the most important state for a directory
                        if (oldState == ItemVersion::ConflictingVersion)
                            continue;
                        if (oldState == ItemVersion::LocallyModifiedUnstagedVersion && state != ItemVersion::ConflictingVersion)
                            continue;
                        if (oldState == ItemVersion::LocallyModifiedVersion
                            && state != ItemVersion::LocallyModifiedUnstagedVersion && state != ItemVersion::ConflictingVersion)
                            continue;
                        versionInfoHash.insert(absoluteDirName, state);
                    } else {
                        versionInfoHash.insert(absoluteDirName, state);
                    }
                }
            }
        }
    }

    // save version
    Global::Cache::instance().addVersion(repositoryPath, versionInfoHash);
}

GitVersionController::GitVersionController()
{
    GitVersionWorker *worker { new GitVersionWorker };
    worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &GitVersionController::requestRetrieval, worker, &GitVersionWorker::retrieval);

    m_thread.start();
}

GitVersionController::~GitVersionController()
{
    m_thread.quit();
    m_thread.wait(3000);
}

GitWindowPlugin::GitWindowPlugin()
{
    registerWindowUrlChanged([this](std::uint64_t winId, const std::string &urlString) {
        return windowUrlChanged(winId, urlString);
    });
    registerWindowClosed([this](std::uint64_t winId) {
        windowClosed(winId);
    });
}

void GitWindowPlugin::windowUrlChanged(std::uint64_t winId, const std::string &urlString)
{
    Q_UNUSED(winId)
    const QUrl &url { QString::fromStdString(urlString) };
    if (!url.isValid() || !url.isLocalFile())
        return;

    emit m_controller.requestRetrieval(url);
}

void GitWindowPlugin::windowClosed(std::uint64_t winId)
{
    // TODO: release memory
}
