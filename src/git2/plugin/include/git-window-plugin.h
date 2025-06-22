#pragma once

#include <dfm-extension/window/dfmextwindowplugin.h>
#include <QObject>
#include <memory>

class GitVersionController;

/**
 * @brief Git窗口插件
 * 
 * 负责监控文件管理器窗口的生命周期，特别是最后一个窗口关闭时
 * 调用daemon服务清理所有资源（缓存和监控）
 */
class GitWindowPlugin : public DFMEXT::DFMExtWindowPlugin
{
public:
    GitWindowPlugin();
    ~GitWindowPlugin();

    // DFMExtWindowPlugin接口实现
    void windowOpened(std::uint64_t winId) DFM_FAKE_OVERRIDE;
    void windowClosed(std::uint64_t winId) DFM_FAKE_OVERRIDE;
    void firstWindowOpened(std::uint64_t winId) DFM_FAKE_OVERRIDE;
    void lastWindowClosed(std::uint64_t winId) DFM_FAKE_OVERRIDE;
    void windowUrlChanged(std::uint64_t winId, const std::string &urlString) DFM_FAKE_OVERRIDE;

private:
    void handleLastWindowClosed();
    
private:
    bool m_initialized;
    std::unique_ptr<GitVersionController> m_controller;
};
