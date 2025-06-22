#include "git-version-worker.h"
#include "git-status-cache.h"
#include "git-utils.h"
#include <QProcess>
#include <QStringConverter>
#include <QFileInfo>
#include <QDebug>
#include <QUrl>

GitVersionWorker::GitVersionWorker(QObject *parent)
    : QObject(parent)
{
    qDebug() << "[GitVersionWorker] Worker initialized";
}

GitVersionWorker::~GitVersionWorker()
{
    qDebug() << "[GitVersionWorker] Worker destroyed";
}

void GitVersionWorker::onRetrieval(const QUrl &url)
{
    if (!url.isValid() || !url.isLocalFile()) {
        return;
    }
    
    onRetrieval(url.toLocalFile());
}

void GitVersionWorker::onRetrieval(const QString &directoryPath)
{
    if (!GitUtils::isInsideRepositoryDir(directoryPath)) {
        return;
    }

    const QString repositoryPath = GitUtils::repositoryBaseDir(directoryPath);
    if (repositoryPath.isEmpty()) {
        qWarning() << "[GitVersionWorker::onRetrieval] Failed to find repository base dir for:" << directoryPath;
        return;
    }

    qDebug() << "[GitVersionWorker::onRetrieval] Processing retrieval for directory:" << directoryPath
             << "repository:" << repositoryPath;

    // 执行状态检索
    auto versionInfoHash = retrieval(directoryPath);
    
    // 检查是否为新仓库
    QStringList existingRepos = GitStatusCache::instance().getCachedRepositories();
    if (!existingRepos.contains(repositoryPath)) {
        Q_EMIT newRepositoryAdded(repositoryPath);
    }

    // 更新缓存
    GitStatusCache::instance().resetVersion(repositoryPath, versionInfoHash);
    
    // 发出完成信号
    Q_EMIT retrievalCompleted(repositoryPath, versionInfoHash);
    
    qDebug() << "[GitVersionWorker::onRetrieval] Retrieval completed for repository:" << repositoryPath
             << "with" << versionInfoHash.size() << "entries";
}

QHash<QString, ItemVersion> GitVersionWorker::retrieval(const QString &directory)
{
    // 迁移自原始的retrieval函数
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "--no-optional-locks", "status", "--porcelain", "-z", "-u", "--ignored" });
    
    const QString dirBelowBaseDir = GitUtils::findPathBelowGitBaseDir(directory);
    QHash<QString, ItemVersion> versionInfoHash;

    qDebug() << "[GitVersionWorker::retrieval] Retrieving status for directory:" << directory
             << "dirBelowBaseDir:" << dirBelowBaseDir;
             
    if (!process.waitForStarted(5000)) {
        qWarning() << "[GitVersionWorker::retrieval] Failed to start git process for:" << directory;
        return versionInfoHash;
    }

    while (process.waitForReadyRead(10000)) {
        char buffer[1024];
        while (GitUtils::readUntilZeroChar(&process, buffer, sizeof(buffer)) > 0) {
            const QString line = QString::fromLocal8Bit(buffer);
            
            // 解析Git状态行：X和Y来自`man git-status`表格
            const auto [X, Y, fileName] = GitUtils::parseLineGitStatus(line);
            ItemVersion state = ItemVersion::NormalVersion;
            
            // 重命名文件会在后面直接列出旧文件名，用\0分隔
            if (X == 'R') {
                state = ItemVersion::LocallyModifiedVersion;
                GitUtils::readUntilZeroChar(&process, nullptr, 0); // 丢弃旧文件名
            }
            
            state = GitUtils::parseXYState(state, X, Y);

            // 决定记录哪些文件信息
            if (state == ItemVersion::NormalVersion || !fileName.startsWith(dirBelowBaseDir)) {
                continue;
            }

            // 相对于当前工作目录的文件名
            const QString relativeFileName = fileName.mid(dirBelowBaseDir.length());
            const QString absoluteFileName = directory + "/" + relativeFileName;
            Q_ASSERT(QUrl::fromLocalFile(absoluteFileName).isValid());
            
            // 普通文件，非目录
            versionInfoHash.insert(absoluteFileName, state);

            // 如果文件是子目录的一部分，记录目录状态
            if (relativeFileName.contains('/')) {
                ItemVersion dirState = state;

                // 对于被忽略的文件，其父目录应该显示为忽略状态
                // 但优先级较低，可以被其他状态覆盖
                if (state == ItemVersion::IgnoredVersion) {
                    dirState = ItemVersion::IgnoredVersion;
                } else {
                    // 对于其他状态，保持原有逻辑
                    if (state == ItemVersion::AddedVersion || state == ItemVersion::RemovedVersion) {
                        dirState = ItemVersion::LocallyModifiedVersion;
                    }
                }

                const QStringList absoluteDirNames = GitUtils::makeDirGroup(directory, relativeFileName);
                for (const QString &absoluteDirName : absoluteDirNames) {
                    Q_ASSERT(QUrl::fromLocalFile(absoluteDirName).isValid());
                    
                    if (versionInfoHash.contains(absoluteDirName)) {
                        ItemVersion oldState = versionInfoHash.value(absoluteDirName);

                        // 目录状态优先级（从高到低）：
                        // ConflictingVersion > LocallyModifiedUnstagedVersion > LocallyModifiedVersion > 其他状态 > IgnoredVersion
                        if (oldState == ItemVersion::ConflictingVersion) {
                            continue;
                        }
                        if (oldState == ItemVersion::LocallyModifiedUnstagedVersion && dirState != ItemVersion::ConflictingVersion) {
                            continue;
                        }
                        if (oldState == ItemVersion::LocallyModifiedVersion
                            && dirState != ItemVersion::LocallyModifiedUnstagedVersion && dirState != ItemVersion::ConflictingVersion) {
                            continue;
                        }

                        // 如果旧状态不是IgnoredVersion，但新状态是IgnoredVersion，不覆盖
                        if (oldState != ItemVersion::IgnoredVersion && dirState == ItemVersion::IgnoredVersion) {
                            continue;
                        }

                        versionInfoHash.insert(absoluteDirName, dirState);
                    } else {
                        versionInfoHash.insert(absoluteDirName, dirState);
                    }
                }
            }
        }
    }

    process.waitForFinished(5000);
    
    if (process.exitCode() != 0) {
        qWarning() << "[GitVersionWorker::retrieval] Git process failed for directory:" << directory
                   << "Error:" << process.readAllStandardError();
    }

    // 计算并设置仓库根目录状态
    ItemVersion rootStatus = calculateRepositoryRootStatus(versionInfoHash);
    versionInfoHash.insert(directory, rootStatus);
    
    qDebug() << "[GitVersionWorker::retrieval] Repository root status set to:" << static_cast<int>(rootStatus) 
             << "for:" << directory;
    qDebug() << "[GitVersionWorker::retrieval] Final versionInfoHash contains" << versionInfoHash.size() << "entries";

    return versionInfoHash;
}

ItemVersion GitVersionWorker::calculateRepositoryRootStatus(const QHash<QString, ItemVersion> &versionInfoHash)
{
    // 迁移自原始的calculateRepositoryRootStatus函数
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
            return ItemVersion::ConflictingVersion; // 最高优先级，直接返回
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