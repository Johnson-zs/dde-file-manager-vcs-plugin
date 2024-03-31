#include "gitrepowatcher.h"

#include <QDir>

GitRepoWatcher::GitRepoWatcher(QObject *parent)
    : QObject { parent }
{
}

void GitRepoWatcher::startWatching(const QString &path)
{
    // if (!m_fileWatcher)
    //     m_fileWatcher = new QFileSystemWatcher(this);
    // if (!m_fileWatcher->addPath(path))
    //     return;

    // connect(
    //         m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &GitRepoWatcher::handleFileChanged, Qt::DirectConnection);
    // connect(
    //         m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &GitRepoWatcher::handleFileChanged, Qt::DirectConnection);

    // // 获取子目录列表
    // QDir dir { path };
    // const QStringList &subDirs { dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks) };

    // // 递归监视子目录
    // for (const QString &subDir : subDirs) {
    //     const QString &fullPath { QDir(path).filePath(subDir) };
    //     startWatching(fullPath);
    // }
}

void GitRepoWatcher::handleFileChanged(const QString &path)
{
    Q_UNUSED(path)
    emit gitRepoChanged();
}
