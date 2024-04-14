#include "gitwindowplugin.h"

#include <QUrl>
#include <QProcess>
#include <QTextCodec>
#include <QFileInfo>
#include <QCoreApplication>

#include <cache.h>

#include "utils.h"

using Global::ItemVersion;

static QHash<QString, Global::ItemVersion> retrieval(const QString &directory)
{
    // cache git status for current path
    QProcess process;
    process.setWorkingDirectory(directory);
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
            const QString &absoluteFileName { directory + "/" + relativeFileName };
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

    return versionInfoHash;
}

void GitVersionWorker::onRetrieval(const QUrl &url)
{
    if (!Utils::isInsideRepositoryDir(url.toLocalFile()))
        return;

    const QString &directory { url.toLocalFile() };
    const QString &repositoryPath { Utils::repositoryBaseDir(directory) };
    if (Q_UNLIKELY(repositoryPath.isEmpty()))
        return;

    // retrival
    const auto &versionInfoHash { ::retrieval(directory) };

    if (!Global::Cache::instance().allRepositoryPaths().contains(repositoryPath))
        emit newRepositoryAdded(repositoryPath);

    // reset version
    Global::Cache::instance().resetVersion(repositoryPath, versionInfoHash);
}

GitVersionController::GitVersionController()
{
    Q_ASSERT(qApp->thread() == QThread::currentThread());
    Q_ASSERT(!m_timer);

    GitVersionWorker *worker { new GitVersionWorker };
    worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &GitVersionController::requestRetrieval,
            worker, &GitVersionWorker::onRetrieval, Qt::QueuedConnection);
    connect(worker, &GitVersionWorker::newRepositoryAdded,
            this, &GitVersionController::onNewRepositoryAdded, Qt::QueuedConnection);

    m_thread.start();
}

GitVersionController::~GitVersionController()
{
    m_thread.quit();
    m_thread.wait(3000);
}

void GitVersionController::onNewRepositoryAdded(const QString &path)
{
    Q_UNUSED(path);

    if (m_timer)
        return;

    // init timer
    static std::once_flag flag;
    std::call_once(flag, [this]() {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &GitVersionController::onTimeout);
        m_timer->start(2000);
    });
}

void GitVersionController::onTimeout()
{
    const auto &paths { Global::Cache::instance().allRepositoryPaths() };
    std::for_each(paths.begin(), paths.end(), [this](const auto &path) {
        const QUrl &url { QUrl::fromLocalFile(path) };
        if (url.isValid())
            emit requestRetrieval(url);
    });
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

    if (!m_controller)
        m_controller.reset(new GitVersionController);
    emit m_controller->requestRetrieval(url);

    // TODO: remove ignroed dir
}

void GitWindowPlugin::windowClosed(std::uint64_t winId)
{
    // TODO: release memory
}
