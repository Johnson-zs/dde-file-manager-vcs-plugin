#ifndef GITMENUPLUGIN_H
#define GITMENUPLUGIN_H

#include <dfm-extension/menu/dfmextmenuplugin.h>
#include <dfm-extension/menu/dfmextaction.h>

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

    // 新增的Git操作函数
    void handleGitDiff(const std::string &filePath);
    void handleGitBlame(const std::string &filePath);
    void handleGitStatus(const std::string &repositoryPath);
    void handleMultiFileAdd(const std::list<std::string> &pathList);
    void handleMultiFileRemove(const std::list<std::string> &pathList);
    void handleMultiFileRevert(const std::list<std::string> &pathList);

    // 工具函数
    void executeGitOperation(const QString &operation, const QString &workingDir, const QStringList &arguments);
    void refreshFileManager();
    
    // Git操作执行策略函数
    void executeSilentGitOperation(const QString &operation, const QString &workingDir, const QStringList &arguments);
    void executeInteractiveGitOperation(const QString &operation, const QString &workingDir, const QStringList &arguments);
    void showSuccessNotification(const QString &operation);
    
    // 新增的菜单辅助函数
    QStringList getCompatibleOperationsForMultiSelection(const std::list<std::string> &pathList);
    bool buildMultiFileMenu(dfmext::DFMExtMenu *gitSubmenu, const std::list<std::string> &pathList, 
                           bool &hasValidAction, dfmext::DFMExtMenu *main, dfmext::DFMExtAction *rootAction);

private:
    DFMEXT::DFMExtMenuProxy *m_proxy { nullptr };
};

#endif   // GITMENUPLUGIN_H
