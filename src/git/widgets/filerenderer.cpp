#include "filerenderer.h"
#include "markdownrenderer.h"

#include <QDebug>

std::unique_ptr<IFileRenderer> FileRendererFactory::createRenderer(const QString &filePath)
{
    qDebug() << "[FileRendererFactory] Creating renderer for file:" << filePath;
    
    // 尝试创建 Markdown 渲染器
    auto markdownRenderer = std::make_unique<MarkdownRenderer>();
    if (markdownRenderer->canRender(filePath)) {
        qInfo() << "INFO: [FileRendererFactory] Created Markdown renderer for:" << filePath;
        return std::move(markdownRenderer);
    }
    
    // 未来可以在这里添加其他类型的渲染器
    // 例如：
    // auto imageRenderer = std::make_unique<ImageRenderer>();
    // if (imageRenderer->canRender(filePath)) {
    //     return std::move(imageRenderer);
    // }
    
    qDebug() << "[FileRendererFactory] No suitable renderer found for:" << filePath;
    return nullptr;
}

bool FileRendererFactory::hasRenderer(const QString &filePath)
{
    // 检查是否有 Markdown 渲染器
    MarkdownRenderer markdownRenderer;
    if (markdownRenderer.canRender(filePath)) {
        return true;
    }
    
    // 未来可以在这里添加其他类型的渲染器检查
    
    return false;
} 