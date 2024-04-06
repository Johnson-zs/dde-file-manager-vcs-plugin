#ifndef GITWINDOWPLUGIN_H
#define GITWINDOWPLUGIN_H

#include <dfm-extension/window/dfmextwindowplugin.h>

#include <QString>
#include <QThread>

#include "gitrepowatcher.h"

class GitVersionWorker : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void retrieval(const QUrl &url);
};

class GitVersionController : public QObject
{
    Q_OBJECT

public:
    GitVersionController();
    ~GitVersionController();

Q_SIGNALS:
    void requestRetrieval(const QUrl &url);

private:
    QThread m_thread;
};

class GitWindowPlugin : public DFMEXT::DFMExtWindowPlugin
{
public:
    GitWindowPlugin();

    void windowUrlChanged(std::uint64_t winId,
                          const std::string &urlString) DFM_FAKE_OVERRIDE;
    void windowClosed(std::uint64_t winId) DFM_FAKE_OVERRIDE;

private:
    GitVersionController m_controller;
    GitRepoWatcher m_repoWatcher;
};

#endif   // GITWINDOWPLUGIN_H
