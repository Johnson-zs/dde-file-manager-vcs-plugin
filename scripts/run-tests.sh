#!/bin/bash

# DDE File Manager Git Plugin - 测试运行脚本
# 用于本地开发和CI环境中运行单元测试

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 默认配置
BUILD_DIR="build"
BUILD_TYPE="Debug"
COVERAGE=false
VERBOSE=false
CLEAN=false
PARALLEL_JOBS=$(nproc)

# 帮助信息
show_help() {
    echo "DDE File Manager Git Plugin - 测试运行脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -h, --help          显示此帮助信息"
    echo "  -c, --clean         清理构建目录"
    echo "  -t, --type TYPE     构建类型 (Debug|Release) [默认: Debug]"
    echo "  -j, --jobs N        并行编译任务数 [默认: $(nproc)]"
    echo "  --coverage          启用代码覆盖率分析"
    echo "  -v, --verbose       详细输出"
    echo "  --build-dir DIR     指定构建目录 [默认: build]"
    echo ""
    echo "示例:"
    echo "  $0                  # 运行基础测试"
    echo "  $0 --coverage       # 运行测试并生成覆盖率报告"
    echo "  $0 --clean -t Release  # 清理构建并运行Release模式测试"
}

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -j|--jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --coverage)
            COVERAGE=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        *)
            echo -e "${RED}错误: 未知选项 '$1'${NC}"
            echo "使用 '$0 --help' 查看帮助信息"
            exit 1
            ;;
    esac
done

# 打印配置信息
print_config() {
    echo -e "${BLUE}=== 测试配置 ===${NC}"
    echo "构建目录: $BUILD_DIR"
    echo "构建类型: $BUILD_TYPE"
    echo "并行任务: $PARALLEL_JOBS"
    echo "代码覆盖率: $([ "$COVERAGE" = true ] && echo "启用" || echo "禁用")"
    echo "详细输出: $([ "$VERBOSE" = true ] && echo "启用" || echo "禁用")"
    echo ""
}

# 检查依赖
check_dependencies() {
    echo -e "${BLUE}=== 检查依赖 ===${NC}"
    
    local missing_deps=()
    
    # 检查基础工具
    command -v cmake >/dev/null 2>&1 || missing_deps+=("cmake")
    command -v make >/dev/null 2>&1 || missing_deps+=("make")
    command -v git >/dev/null 2>&1 || missing_deps+=("git")
    
    # 检查Qt6
    if ! pkg-config --exists Qt6Core Qt6Widgets Qt6DBus Qt6Test; then
        missing_deps+=("Qt6开发包")
    fi
    
    # 检查覆盖率工具（如果需要）
    if [ "$COVERAGE" = true ]; then
        command -v lcov >/dev/null 2>&1 || missing_deps+=("lcov")
        command -v gcov >/dev/null 2>&1 || missing_deps+=("gcov")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        echo -e "${RED}错误: 缺少以下依赖:${NC}"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        echo ""
        echo "在Ubuntu/Debian上，可以运行以下命令安装依赖:"
        echo "sudo apt-get install cmake build-essential qt6-base-dev qt6-tools-dev libqt6dbus6-dev git lcov"
        exit 1
    fi
    
    echo -e "${GREEN}✓ 所有依赖已满足${NC}"
    echo ""
}

# 清理构建目录
clean_build() {
    if [ "$CLEAN" = true ]; then
        echo -e "${YELLOW}=== 清理构建目录 ===${NC}"
        if [ -d "$BUILD_DIR" ]; then
            rm -rf "$BUILD_DIR"
            echo -e "${GREEN}✓ 构建目录已清理${NC}"
        else
            echo "构建目录不存在，跳过清理"
        fi
        echo ""
    fi
}

# 配置CMake
configure_cmake() {
    echo -e "${BLUE}=== 配置CMake ===${NC}"
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
        "-DBUILD_TESTS=ON"
    )
    
    if [ "$COVERAGE" = true ]; then
        cmake_args+=("-DCMAKE_CXX_FLAGS=--coverage")
    fi
    
    if [ "$VERBOSE" = true ]; then
        cmake_args+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
    fi
    
    echo "CMake参数: ${cmake_args[*]}"
    
    if cmake "${cmake_args[@]}" ..; then
        echo -e "${GREEN}✓ CMake配置成功${NC}"
    else
        echo -e "${RED}✗ CMake配置失败${NC}"
        exit 1
    fi
    
    cd ..
    echo ""
}

# 编译项目
build_project() {
    echo -e "${BLUE}=== 编译项目 ===${NC}"
    
    cd "$BUILD_DIR"
    
    local make_args=("-j$PARALLEL_JOBS")
    
    if [ "$VERBOSE" = true ]; then
        make_args+=("VERBOSE=1")
    fi
    
    echo "编译参数: make ${make_args[*]}"
    
    if make "${make_args[@]}"; then
        echo -e "${GREEN}✓ 编译成功${NC}"
    else
        echo -e "${RED}✗ 编译失败${NC}"
        exit 1
    fi
    
    cd ..
    echo ""
}

# 运行测试
run_tests() {
    echo -e "${BLUE}=== 运行测试 ===${NC}"
    
    cd "$BUILD_DIR"
    
    # 首先检查是否有测试
    echo "检查可用的测试..."
    if ! ctest --show-only >/dev/null 2>&1; then
        echo -e "${YELLOW}⚠ CTest配置可能有问题${NC}"
    fi
    
    echo "可用的测试:"
    ctest --show-only
    
    local ctest_args=("--output-on-failure")
    
    if [ "$VERBOSE" = true ]; then
        ctest_args+=("--verbose")
    fi
    
    echo ""
    echo "测试参数: ctest ${ctest_args[*]}"
    
    if ctest "${ctest_args[@]}"; then
        echo -e "${GREEN}✓ 所有测试通过${NC}"
    else
        local exit_code=$?
        echo -e "${RED}✗ 部分测试失败 (退出码: $exit_code)${NC}"
        
        echo ""
        echo -e "${YELLOW}=== 失败测试详情 ===${NC}"
        ctest --rerun-failed --output-on-failure
        
        echo ""
        echo -e "${YELLOW}=== 可用的测试可执行文件 ===${NC}"
        find bin -name "test-*" -executable 2>/dev/null || echo "没有找到测试可执行文件"
        
        cd ..
        exit $exit_code
    fi
    
    cd ..
    echo ""
}

# 生成覆盖率报告
generate_coverage() {
    if [ "$COVERAGE" = true ]; then
        echo -e "${BLUE}=== 生成覆盖率报告 ===${NC}"
        
        cd "$BUILD_DIR"
        
        # 捕获覆盖率数据
        if lcov --directory . --capture --output-file coverage.info; then
            echo -e "${GREEN}✓ 覆盖率数据捕获成功${NC}"
        else
            echo -e "${YELLOW}⚠ 覆盖率数据捕获失败${NC}"
            cd ..
            return
        fi
        
        # 过滤系统文件和测试文件
        lcov --remove coverage.info '/usr/*' --output-file coverage.info
        lcov --remove coverage.info '*/tests/*' --output-file coverage.info
        
        # 显示覆盖率摘要
        echo ""
        echo -e "${BLUE}=== 覆盖率摘要 ===${NC}"
        lcov --list coverage.info
        
        # 生成HTML报告
        if command -v genhtml >/dev/null 2>&1; then
            if genhtml coverage.info --output-directory coverage_report; then
                echo ""
                echo -e "${GREEN}✓ HTML覆盖率报告已生成: $BUILD_DIR/coverage_report/index.html${NC}"
            fi
        fi
        
        cd ..
        echo ""
    fi
}

# 显示测试结果摘要
show_summary() {
    echo -e "${BLUE}=== 测试完成 ===${NC}"
    echo -e "${GREEN}✓ 所有步骤已成功完成${NC}"
    
    if [ "$COVERAGE" = true ]; then
        echo ""
        echo "覆盖率报告位置:"
        echo "  - 原始数据: $BUILD_DIR/coverage.info"
        if [ -f "$BUILD_DIR/coverage_report/index.html" ]; then
            echo "  - HTML报告: $BUILD_DIR/coverage_report/index.html"
        fi
    fi
    
    echo ""
    echo "要重新运行测试，请执行:"
    echo "  cd $BUILD_DIR && ctest"
    echo ""
}

# 主函数
main() {
    echo -e "${GREEN}DDE File Manager Git Plugin - 单元测试${NC}"
    echo ""
    
    print_config
    check_dependencies
    clean_build
    configure_cmake
    build_project
    run_tests
    generate_coverage
    show_summary
}

# 运行主函数
main "$@" 