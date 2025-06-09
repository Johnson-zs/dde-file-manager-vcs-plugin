#ifndef GITMENUBUILDER_H
#define GITMENUBUILDER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <list>
#include <string>

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>
#include <dfm-extension/menu/dfmextaction.h>

class GitOperationService;

/**
 * @brief Git菜单构建器
 *
 * 负责根据文件状态和上下文构建相应的Git菜单项
 * 分离菜单构建逻辑，提高代码可维护性
 */
class GitMenuBuilder : public QObject
{
    Q_OBJECT

public:
    explicit GitMenuBuilder(DFMEXT::DFMExtMenuProxy *proxy,
                            GitOperationService *operationService,
                            QObject *parent = nullptr);
    ~GitMenuBuilder();

    /**
     * @brief 构建单文件菜单
     */
    bool buildSingleFileMenu(DFMEXT::DFMExtMenu *gitSubmenu,
                             const QString &currentPath,
                             const QString &focusPath);

    /**
     * @brief 构建多文件菜单
     */
    bool buildMultiFileMenu(DFMEXT::DFMExtMenu *gitSubmenu,
                            const std::list<std::string> &pathList);

    /**
     * @brief 构建仓库菜单项（直接添加到主菜单）
     */
    bool buildRepositoryMenuItems(DFMEXT::DFMExtMenu *main,
                                  const QString &repositoryPath,
                                  DFMEXT::DFMExtAction *beforeAction = nullptr);

private:
    // === 菜单项创建方法 ===
    void addFileOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &filePath);
    void addViewOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &filePath,
                                   const QString &currentPath);
    void addRepositoryOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &repositoryPath,
                                         DFMEXT::DFMExtAction *beforeAction = nullptr);
    void addBranchOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &repositoryPath);
    void addSyncOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &repositoryPath);
    void addStashOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &repositoryPath);

    // === 多文件操作辅助 ===
    QStringList getCompatibleOperationsForMultiSelection(const std::list<std::string> &pathList);
    void addMultiFileOperationMenuItems(DFMEXT::DFMExtMenu *menu,
                                        const std::list<std::string> &pathList,
                                        const QStringList &compatibleOps);

    // === 工具方法 ===
    DFMEXT::DFMExtAction *createSeparator();
    QString getFileCountText(int count) const;

    DFMEXT::DFMExtMenuProxy *m_proxy;
    GitOperationService *m_operationService;
};

#endif   // GITMENUBUILDER_H
