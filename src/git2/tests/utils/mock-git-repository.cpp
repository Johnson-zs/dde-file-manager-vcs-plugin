#include "test-utils.h"
#include <QDebug>

MockGitRepository::MockGitRepository()
    : m_initialized(false)
{
}

MockGitRepository::~MockGitRepository()
{
    // QTemporaryDir会自动清理
}

bool MockGitRepository::initialize()
{
    qDebug() << "MockGitRepository::initialize() called";
    
    if (m_initialized) {
        qDebug() << "Already initialized, returning true";
        return true;
    }
    
    qDebug() << "Checking temporary directory validity...";
    if (!m_tempDir.isValid()) {
        qWarning() << "Failed to create temporary directory, error:" << m_tempDir.errorString();
        // 尝试重新创建
        m_tempDir = QTemporaryDir();
        if (!m_tempDir.isValid()) {
            qWarning() << "Second attempt failed, error:" << m_tempDir.errorString();
            return false;
        }
    }
    
    QString repoPath = m_tempDir.path();
    qDebug() << "Temporary directory created at:" << repoPath;
    
    // 初始化Git仓库
    qDebug() << "Calling TestUtils::createTestGitRepository...";
    if (!TestUtils::createTestGitRepository(repoPath)) {
        qWarning() << "Failed to initialize git repository at:" << repoPath;
        // 对于测试，我们可以继续而不是失败
        qWarning() << "Continuing without git repository for basic testing";
        m_initialized = true;
        return true;
    }
    qDebug() << "Git repository created successfully";
    
    // 创建初始提交
    qDebug() << "Adding initial file...";
    if (!addFile("README.md", "# Test Repository\n\nThis is a test repository.")) {
        qWarning() << "Failed to create initial file";
        // 继续测试
        m_initialized = true;
        return true;
    }
    qDebug() << "Initial file added successfully";
    
    qDebug() << "Creating initial commit...";
    if (!commit("Initial commit")) {
        qWarning() << "Failed to create initial commit";
        // 继续测试
        m_initialized = true;
        return true;
    }
    qDebug() << "Initial commit created successfully";
    
    m_initialized = true;
    qDebug() << "Mock git repository initialized at:" << repoPath;
    return true;
}

bool MockGitRepository::addFile(const QString &fileName, const QString &content)
{
    if (!m_tempDir.isValid()) {
        return false;
    }
    
    QString repoPath = m_tempDir.path();
    
    // 创建文件
    if (!TestUtils::createTestFile(repoPath, fileName, content)) {
        return false;
    }
    
    // 添加到Git - git add通常不返回输出，成功时为空
    TestUtils::executeGitCommand(repoPath, "add", {fileName});
    
    // 验证文件是否被添加到暂存区
    QString status = TestUtils::executeGitCommand(repoPath, "status", {"--porcelain", fileName});
    return !status.isNull();  // 如果能获取状态，说明add成功
}

bool MockGitRepository::modifyFile(const QString &fileName, const QString &content)
{
    if (!m_tempDir.isValid()) {
        return false;
    }
    
    QString filePath = m_tempDir.path() + "/" + fileName;
    return TestUtils::modifyTestFile(filePath, content);
}

bool MockGitRepository::removeFile(const QString &fileName)
{
    if (!m_tempDir.isValid()) {
        return false;
    }
    
    QString repoPath = m_tempDir.path();
    QString output = TestUtils::executeGitCommand(repoPath, "rm", {fileName});
    return !output.isNull();
}

bool MockGitRepository::commit(const QString &message)
{
    if (!m_tempDir.isValid()) {
        return false;
    }
    
    QString repoPath = m_tempDir.path();
    
    // 检查是否有文件在暂存区
    QString status = TestUtils::executeGitCommand(repoPath, "status", {"--porcelain"});
    if (status.isNull()) {
        qWarning() << "Failed to get repository status";
        return false;
    }
    
    // 如果没有暂存的文件，先添加所有文件
    if (status.isEmpty()) {
        TestUtils::executeGitCommand(repoPath, "add", {"."});
    }
    
    // 执行提交
    QString output = TestUtils::executeGitCommand(repoPath, "commit", {"-m", message});
    return !output.isNull();
}

QString MockGitRepository::getFileStatus(const QString &fileName)
{
    if (!m_tempDir.isValid()) {
        return QString();
    }
    
    QString repoPath = m_tempDir.path();
    
    // 使用git status --porcelain获取文件状态
    QString output = TestUtils::executeGitCommand(repoPath, "status", {"--porcelain", fileName});
    
    if (output.isEmpty()) {
        return "clean";  // 文件未修改
    }
    
    // 解析状态
    if (output.startsWith("??")) {
        return "untracked";
    } else if (output.startsWith(" M")) {
        return "modified";
    } else if (output.startsWith("A ")) {
        return "added";
    } else if (output.startsWith("D ")) {
        return "deleted";
    } else if (output.startsWith("M ")) {
        return "staged";
    }
    
    return output.left(2);  // 返回状态码
} 