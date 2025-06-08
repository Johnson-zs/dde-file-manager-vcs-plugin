#ifndef GITWINDOWPLUGIN_H
#define GITWINDOWPLUGIN_H

#include <dfm-extension/window/dfmextwindowplugin.h>

#include <QString>
#include <QThread>
#include <QTimer>

class GitFileSystemWatcher;

class GitVersionWorker : public QObject
{
    Q_OBJECT

Q_SIGNALS:
    void newRepositoryAdded(const QString &path);

public Q_SLOTS:
    void onRetrieval(const QUrl &url);
};

class GitVersionController : public QObject
{
    Q_OBJECT

public:
    GitVersionController();
    ~GitVersionController();

Q_SIGNALS:
    void requestRetrieval(const QUrl &url);

private Q_SLOTS:
    void onNewRepositoryAdded(const QString &path);
    void onTimeout();
    void onRepositoryChanged(const QString &repositoryPath);
    void onRepositoryUpdateRequested(const QString &repositoryPath);

private:
    QThread m_thread;
    QTimer *m_timer { nullptr };
    GitFileSystemWatcher *m_fileSystemWatcher { nullptr };
    bool m_useFileSystemWatcher { true };
};

class GitWindowPlugin : public DFMEXT::DFMExtWindowPlugin
{
public:
    GitWindowPlugin();

    void windowUrlChanged(std::uint64_t winId,
                          const std::string &urlString) DFM_FAKE_OVERRIDE;
    void firstWindowOpened(std::uint64_t winId) DFM_FAKE_OVERRIDE;
    void windowClosed(std::uint64_t winId) DFM_FAKE_OVERRIDE;

private:
    std::unique_ptr<GitVersionController> m_controller;
};

#endif   // GITWINDOWPLUGIN_H
