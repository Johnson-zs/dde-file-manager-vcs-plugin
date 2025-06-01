#ifndef FILERENDERER_H
#define FILERENDERER_H

#include <QWidget>
#include <QString>
#include <memory>

/**
 * @brief 文件渲染器抽象接口
 * 
 * 定义了文件内容渲染的通用接口，支持不同类型的文件渲染器实现。
 * 遵循单一职责原则和开闭原则。
 */
class IFileRenderer
{
public:
    virtual ~IFileRenderer() = default;
    
    /**
     * @brief 检查是否支持指定的文件类型
     * @param filePath 文件路径
     * @return 如果支持返回 true，否则返回 false
     */
    virtual bool canRender(const QString &filePath) const = 0;
    
    /**
     * @brief 创建渲染器的 Widget
     * @param parent 父窗口
     * @return 渲染器 Widget 指针
     */
    virtual QWidget* createWidget(QWidget *parent = nullptr) = 0;
    
    /**
     * @brief 设置要渲染的文件内容
     * @param content 文件内容
     */
    virtual void setContent(const QString &content) = 0;
    
    /**
     * @brief 获取渲染器类型名称
     * @return 渲染器类型名称
     */
    virtual QString getRendererType() const = 0;
    
    /**
     * @brief 检查是否支持视图模式切换
     * @return 如果支持返回 true，否则返回 false
     */
    virtual bool supportsViewToggle() const { return false; }
    
    /**
     * @brief 切换视图模式（如果支持）
     */
    virtual void toggleViewMode() {}
    
    /**
     * @brief 获取当前视图模式描述
     * @return 视图模式描述
     */
    virtual QString getCurrentViewModeDescription() const { return QString(); }
};

/**
 * @brief 文件渲染器工厂
 * 
 * 负责根据文件类型创建合适的渲染器实例。
 * 遵循工厂模式和依赖倒置原则。
 */
class FileRendererFactory
{
public:
    /**
     * @brief 创建适合指定文件的渲染器
     * @param filePath 文件路径
     * @return 渲染器智能指针，如果没有合适的渲染器则返回 nullptr
     */
    static std::unique_ptr<IFileRenderer> createRenderer(const QString &filePath);
    
    /**
     * @brief 检查是否有适合指定文件的渲染器
     * @param filePath 文件路径
     * @return 如果有合适的渲染器返回 true，否则返回 false
     */
    static bool hasRenderer(const QString &filePath);
    
private:
    FileRendererFactory() = default;
};

#endif // FILERENDERER_H 