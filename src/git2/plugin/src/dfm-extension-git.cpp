#include <dfm-extension/dfm-extension.h>

// TODO: 实现插件入口点
// 这个文件将实现文件管理器插件的注册逻辑

extern "C" {
    __attribute__((visibility("default"))) void dfm_extension_initalize() {
        // TODO: 初始化Git插件
    }
    
    __attribute__((visibility("default"))) void dfm_extension_shutdown() {
        // TODO: 清理Git插件资源
    }
} 