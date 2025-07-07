#include "gitwindowplugin.h"
#include "gitfilesystemwatcher.h"

#include <QUrl>
#include <QProcess>
#include <QTextCodec>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>

#include <cache.h>

#include "utils.h"
#include "common/gitrepositoryservice.h"

using Global::ItemVersion;

// 计算仓库根目录状态的辅助函数
static ItemVersion calculateRepositoryRootStatus(const QHash<QString, ItemVersion> &versionInfoHash)
{
    if (versionInfoHash.isEmpty()) {
        return ItemVersion::NormalVersion;
    }

    ItemVersion rootState = ItemVersion::NormalVersion;

    // 遍历所有状态，找出最高优先级的状态
    for (auto it = versionInfoHash.begin(); it != versionInfoHash.end(); ++it) {
        ItemVersion currentState = it.value();

        // 忽略IgnoredVersion，它不应该影响根目录状态
        if (currentState == ItemVersion::IgnoredVersion) {
            continue;
        }

        // 状态优先级：ConflictingVersion > LocallyModifiedUnstagedVersion > LocallyModifiedVersion > 其他状态
        if (currentState == ItemVersion::ConflictingVersion) {
            return ItemVersion::ConflictingVersion;   // 最高优先级，直接返回
        } else if (currentState == ItemVersion::LocallyModifiedUnstagedVersion && rootState != ItemVersion::ConflictingVersion) {
            rootState = ItemVersion::LocallyModifiedUnstagedVersion;
        } else if (currentState == ItemVersion::LocallyModifiedVersion && rootState != ItemVersion::ConflictingVersion && rootState != ItemVersion::LocallyModifiedUnstagedVersion) {
            rootState = ItemVersion::LocallyModifiedVersion;
        } else if (rootState == ItemVersion::NormalVersion) {
            // 如果根目录状态还是Normal，则使用当前状态
            rootState = currentState;
        }
    }

    return rootState;
}

static QHash<QString, Global::ItemVersion> retrieval(const QString &directory)
{
    // cache git status for current path
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "--no-optional-locks", "status", "--porcelain", "-z", "-u", "--ignored" });
    const QString &dirBelowBaseDir { Utils::findPathBelowGitBaseDir(directory) };
    QHash<QString, ItemVersion> versionInfoHash;

    qDebug() << "[GitVersionWorker] Retrieving status for directory:" << directory
             << "dirBelowBaseDir:" << dirBelowBaseDir;
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
                ItemVersion dirState = state;

                // 对于被忽略的文件，其父目录应该显示为忽略状态
                // 但优先级较低，可以被其他状态覆盖
                if (state == ItemVersion::IgnoredVersion) {
                    dirState = ItemVersion::IgnoredVersion;
                } else {
                    // 对于其他状态，保持原有逻辑
                    if (state == ItemVersion::AddedVersion || state == ItemVersion::RemovedVersion)
                        dirState = ItemVersion::LocallyModifiedVersion;
                }

                const QStringList &absoluteDirNames { Utils::makeDirGroup(directory, relativeFileName) };
                for (const auto &absoluteDirName : absoluteDirNames) {
                    Q_ASSERT(QUrl::fromLocalFile(absoluteDirName).isValid());
                    if (versionInfoHash.contains(absoluteDirName)) {
                        ItemVersion oldState = versionInfoHash.value(absoluteDirName);

                        // 目录状态优先级（从高到低）：
                        // ConflictingVersion > LocallyModifiedUnstagedVersion > LocallyModifiedVersion > 其他状态 > IgnoredVersion
                        if (oldState == ItemVersion::ConflictingVersion)
                            continue;
                        if (oldState == ItemVersion::LocallyModifiedUnstagedVersion && dirState != ItemVersion::ConflictingVersion)
                            continue;
                        if (oldState == ItemVersion::LocallyModifiedVersion
                            && dirState != ItemVersion::LocallyModifiedUnstagedVersion && dirState != ItemVersion::ConflictingVersion)
                            continue;

                        // 如果旧状态不是IgnoredVersion，但新状态是IgnoredVersion，不覆盖
                        if (oldState != ItemVersion::IgnoredVersion && dirState == ItemVersion::IgnoredVersion)
                            continue;

                        versionInfoHash.insert(absoluteDirName, dirState);
                    } else {
                        versionInfoHash.insert(absoluteDirName, dirState);
                    }
                }
            }
        }
    }

    // 计算并设置仓库根目录状态
    ItemVersion rootStatus = calculateRepositoryRootStatus(versionInfoHash);
    versionInfoHash.insert(directory, rootStatus);
    qDebug() << "[GitVersionWorker] Repository root status set to:" << static_cast<int>(rootStatus) << "for:" << directory;

    qDebug() << "[GitVersionWorker] Final versionInfoHash contains" << versionInfoHash.size() << "entries";

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
    auto versionInfoHash { ::retrieval(directory) };
    // 关键修复：不要在versionInfoHash为空时插入NormalVersion
    // 空的versionInfoHash意味着没有任何文件状态变化，这是正常的
    // 让Global::Cache来处理缺失的条目，它会正确返回NormalVersion

    if (!Global::Cache::instance().allRepositoryPaths().contains(repositoryPath))
        emit newRepositoryAdded(repositoryPath);

    // reset version
    Global::Cache::instance().resetVersion(repositoryPath, versionInfoHash);
}

GitVersionController::GitVersionController()
{
    Q_ASSERT(qApp->thread() == QThread::currentThread());
    Q_ASSERT(!m_timer);

    qInfo() << "INFO: [GitVersionController] Initializing with real-time file system monitoring";

    GitVersionWorker *worker { new GitVersionWorker };
    worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &GitVersionController::requestRetrieval,
            worker, &GitVersionWorker::onRetrieval, Qt::QueuedConnection);
    connect(worker, &GitVersionWorker::newRepositoryAdded,
            this, &GitVersionController::onNewRepositoryAdded, Qt::QueuedConnection);

    // 连接到公共仓库服务
    connect(&GitRepositoryService::instance(), &GitRepositoryService::repositoryUpdateRequested,
            this, &GitVersionController::onRepositoryUpdateRequested, Qt::QueuedConnection);

    // 初始化文件系统监控器
    if (m_useFileSystemWatcher) {
        m_fileSystemWatcher = new GitFileSystemWatcher(this);
        connect(m_fileSystemWatcher, &GitFileSystemWatcher::repositoryChanged,
                this, &GitVersionController::onRepositoryChanged, Qt::QueuedConnection);

        qInfo() << "INFO: [GitVersionController] Real-time file system watcher enabled";
    }

    m_thread.start();
}

GitVersionController::~GitVersionController()
{
    m_thread.quit();
    m_thread.wait(3000);
}

void GitVersionController::onNewRepositoryAdded(const QString &path)
{
    qInfo() << "INFO: [GitVersionController] New repository added:" << path;

    // 添加到文件系统监控器
    if (m_fileSystemWatcher) {
        m_fileSystemWatcher->addRepository(path);
        qInfo() << "INFO: [GitVersionController] Added repository to file system watcher:" << path;
    }

    // 保留定时器作为备用机制（降低频率）
    if (m_timer)
        return;

    // init timer with longer interval as backup
    static std::once_flag flag;
    std::call_once(flag, [this]() {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &GitVersionController::onTimeout);
        // 使用更长的间隔作为备用机制（30秒）
        int timerInterval = m_useFileSystemWatcher ? 30000 : 2000;
        m_timer->start(timerInterval);

        qInfo() << "INFO: [GitVersionController] Backup timer started with interval:" << timerInterval << "ms";
    });
}

void GitVersionController::onTimeout()
{
    qDebug() << "[GitVersionController] Backup timer triggered - performing periodic update";

    const auto &paths { Global::Cache::instance().allRepositoryPaths() };
    std::for_each(paths.begin(), paths.end(), [this](const auto &path) {
        const QUrl &url { QUrl::fromLocalFile(path) };
        if (url.isValid())
            emit requestRetrieval(url);
    });
}

void GitVersionController::onRepositoryChanged(const QString &repositoryPath)
{
    qInfo() << "INFO: [GitVersionController] Repository changed detected:" << repositoryPath;

    // 立即触发状态更新
    const QUrl &url { QUrl::fromLocalFile(repositoryPath) };
    if (url.isValid()) {
        emit requestRetrieval(url);
        qDebug() << "[GitVersionController] Triggered immediate update for repository:" << repositoryPath;
    }
}

void GitVersionController::onRepositoryUpdateRequested(const QString &repositoryPath)
{
    qInfo() << "INFO: [GitVersionController] Repository update requested from service:" << repositoryPath;

    // 通过服务请求触发状态更新
    const QUrl &url { QUrl::fromLocalFile(repositoryPath) };
    if (url.isValid()) {
        emit requestRetrieval(url);
        qDebug() << "[GitVersionController] Triggered service-requested update for repository:" << repositoryPath;
    }
}

GitWindowPlugin::GitWindowPlugin()
{
    registerFirstWindowOpened([this](std::uint64_t winId) {
        firstWindowOpened(winId);
    });
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

void GitWindowPlugin::firstWindowOpened(uint64_t winId)
{
    Q_UNUSED(winId)
    if (!m_controller)
        m_controller.reset(new GitVersionController);
}

void GitWindowPlugin::windowClosed(std::uint64_t winId)
{
    // TODO: release memory
}
