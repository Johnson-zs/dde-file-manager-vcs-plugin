# Git插件单元测试框架
project(dde-file-manager-git-tests)

# 启用测试
enable_testing()

# Qt6 Test组件
find_package(Qt6 REQUIRED COMPONENTS Test Concurrent)

# 设置测试包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/git2/common/include
    ${CMAKE_SOURCE_DIR}/src/git2/daemon/include
    ${CMAKE_SOURCE_DIR}/src/git2/plugin/include
)

# 测试工具函数库
set(TEST_UTILS_SOURCES
    utils/test-utils.cpp
    utils/mock-git-repository.cpp
)

add_library(test-utils STATIC ${TEST_UTILS_SOURCES})
target_link_libraries(test-utils
    PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Concurrent
        dde-git-common
)

# 定义测试宏
macro(add_git_test test_name)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name}
        PRIVATE
            Qt6::Core
            Qt6::Test
            Qt6::DBus
            Qt6::Concurrent
            dde-git-common
            dde-git-daemon-lib
            test-utils
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endmacro()

# GitStatusCache测试套件
add_git_test(test-git-status-cache)

# GitVersionWorker测试套件  
add_git_test(test-git-version-worker)

# Git工具函数测试套件
add_git_test(test-git-utils)

# GitStatusParser测试套件
add_git_test(test-git-status-parser)

# DBus接口集成测试
add_git_test(test-git-dbus-service)

# 测试覆盖率配置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    
    # 添加覆盖率目标
    add_custom_target(coverage
        COMMAND lcov --directory . --capture --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND lcov --remove coverage.info '*/tests/*' --output-file coverage.info
        COMMAND genhtml coverage.info --output-directory coverage_report
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating test coverage report"
    )
endif()

# 运行所有测试的目标
add_custom_target(run-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS 
        test-git-status-cache
        test-git-version-worker  
        test-git-utils
        test-git-status-parser
        test-git-dbus-service
    COMMENT "Running all unit tests"
)

# 调试测试
add_git_test(debug_test)
