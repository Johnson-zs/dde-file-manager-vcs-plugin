#ifndef GITSERVICEINTERFACE_H
#define GITSERVICEINTERFACE_H

#include <QString>

/**
 * @brief Git服务接口
 * 
 * 定义了Git仓库管理的核心接口，遵循依赖倒置原则。
 * 插件通过此接口进行仓库发现和状态更新的协调。
 */
class GitServiceInterface
{
public:
    virtual ~GitServiceInterface() = default;
    
    /**
     * @brief 请求仓库状态更新
     * @param repositoryPath 仓库根目录路径
     */
    virtual void requestRepositoryUpdate(const QString &repositoryPath) = 0;
    
    /**
     * @brief 注册新发现的仓库
     * @param repositoryPath 仓库根目录路径
     */
    virtual void registerRepositoryDiscovered(const QString &repositoryPath) = 0;
    
    /**
     * @brief 检查仓库是否已被跟踪
     * @param repositoryPath 仓库根目录路径
     * @return 是否已被跟踪
     */
    virtual bool isRepositoryTracked(const QString &repositoryPath) const = 0;
};

#endif // GITSERVICEINTERFACE_H 