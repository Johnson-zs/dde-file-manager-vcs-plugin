# Git公共组件库
project(dde-git-common)

# 设置头文件
set(COMMON_HEADERS
    include/git-types.h
    include/git-status-parser.h
    include/git-utils.h
)

# 设置源文件
set(COMMON_SOURCES
    src/git-status-parser.cpp
    src/git-utils.cpp
)

# 创建静态库
add_library(${PROJECT_NAME} STATIC
    ${COMMON_HEADERS}
    ${COMMON_SOURCES}
)

# 设置包含目录
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接Qt6库
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt6::Core
)

# 设置编译属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# 安装目标
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# 安装头文件
install(FILES ${COMMON_HEADERS}
    DESTINATION include/dde-git-common
) 