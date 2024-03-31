#ifndef GITREPOWATCHER_H
#define GITREPOWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>

class GitRepoWatcher : public QObject
{
    Q_OBJECT
public:
    explicit GitRepoWatcher(QObject *parent = nullptr);
    void startWatching(const QString &path);

Q_SIGNALS:
    void gitRepoChanged();

private Q_SLOTS:
    void handleFileChanged(const QString &path);

private:
    QFileSystemWatcher *m_fileWatcher { nullptr };   // TODO:
};

#endif   // GITREPOWATCHER_H
