#ifndef GITMENUPLUGIN_H
#define GITMENUPLUGIN_H

#include <dfm-extension/menu/dfmextmenuplugin.h>

#include <QString>

class GitMenuPlugin : public DFMEXT::DFMExtMenuPlugin
{
public:
    GitMenuPlugin();

    void initialize(DFMEXT::DFMExtMenuProxy *proxy) DFM_FAKE_OVERRIDE;
    bool buildNormalMenu(DFMEXT::DFMExtMenu *main,
                         const std::string &currentPath,
                         const std::string &focusPath,
                         const std::list<std::string> &pathList,
                         bool onDesktop) DFM_FAKE_OVERRIDE;
    bool buildEmptyAreaMenu(DFMEXT::DFMExtMenu *main, const std::string &currentPath, bool onDesktop) DFM_FAKE_OVERRIDE;

private:
    // Git操作处理函数
    void handleGitAdd(const std::string &filePath);
    void handleGitRemove(const std::string &filePath);
    void handleGitRevert(const std::string &filePath);
    void handleGitLog(const std::string &repositoryPath, const std::string &filePath = std::string());
    void handleGitCheckout(const std::string &repositoryPath);
    void handleGitPush(const std::string &repositoryPath);
    void handleGitPull(const std::string &repositoryPath);
    void handleGitCommit(const std::string &repositoryPath);

    // 工具函数
    void executeGitOperation(const QString &operation, const QString &workingDir, const QStringList &arguments);
    void refreshFileManager();

private:
    DFMEXT::DFMExtMenuProxy *m_proxy { nullptr };
};

#endif   // GITMENUPLUGIN_H
