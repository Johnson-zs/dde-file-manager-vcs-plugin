#ifndef GITDIALOGS_H
#define GITDIALOGS_H

/**
 * @brief Git对话框模块统一入口
 * 
 * 包含所有Git相关对话框的声明
 * 提供统一的包含接口
 */

// 核心对话框
#include "gitlogdialog.h"
#include "gitoperationdialog.h"

// 其他功能对话框
#include "gitcheckoutdialog.h"
#include "gitcommitdialog.h"
#include "gitdiffdialog.h"
#include "gitstatusdialog.h"
#include "gitblamedialog.h"

#endif // GITDIALOGS_H 