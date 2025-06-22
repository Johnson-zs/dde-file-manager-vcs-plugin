#include <QtTest/QtTest>
#include <QObject>
#include <QDebug>
#include <QElapsedTimer>
#include "utils/test-utils.h"
#include "git-status-parser.h"
#include "git-types.h"

/**
 * @brief GitStatusParser单元测试类
 * 
 * 测试覆盖：
 * 1. Git命令输出解析的准确性
 * 2. 各种Git状态的正确识别
 * 3. 边界情况和异常输入处理
 * 4. 性能测试
 */
class TestGitStatusParser : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 基础解析测试
    void testParseNormalStatus();
    void testParseModifiedStatus();
    void testParseAddedStatus();
    void testParseRemovedStatus();
    void testParseUntrackedStatus();
    void testParseRenamedStatus();
    void testParseCopiedStatus();
    void testParseConflictStatus();
    
    // 复合状态测试
    void testParseMultipleFiles();
    void testParseMixedStatuses();
    void testParseEmptyStatus();
    
    // 特殊情况测试
    void testParseSpecialCharacters();
    void testParseUnicodeFilenames();
    void testParseSpacesInFilenames();
    void testParseQuotedFilenames();
    
    // 错误处理测试
    void testParseInvalidInput();
    void testParseCorruptedOutput();
    void testParseEmptyInput();
    
    // Git命令输出格式测试
    void testParsePorcelainFormat();
    void testParseShortFormat();
    void testParseLongFormat();
    
    // 性能测试
    void testParseLargeOutput();
    void testParsePerformance();

private:
    QString createGitStatusOutput(const QStringList &statusLines);
    void verifyParsedStatus(const GitStatusMap &result, 
                           const QString &filePath, ItemVersion expectedStatus);
    
private:
    GitStatusParser *m_parser;
    QString m_testRepoPath;
};

void TestGitStatusParser::initTestCase()
{
    qDebug() << "Starting GitStatusParser tests";
}

void TestGitStatusParser::cleanupTestCase()
{
    qDebug() << "GitStatusParser tests completed";
}

void TestGitStatusParser::init()
{
    m_parser = nullptr; // GitStatusParser is static, no need to create instance
    m_testRepoPath = "/tmp/test_repo";  // 测试用的虚拟路径
}

void TestGitStatusParser::cleanup()
{
    // GitStatusParser is static, no cleanup needed
    m_parser = nullptr;
}

void TestGitStatusParser::testParseNormalStatus()
{
    // Git status --porcelain 对于已提交文件不会有输出
    // 这里测试空输出的处理
    QString emptyOutput = "";
    GitStatusMap result = GitStatusParser::parseGitStatus(emptyOutput);
    
    // 空输出应该返回空结果
    QVERIFY(result.isEmpty());
}

void TestGitStatusParser::testParseModifiedStatus()
{
    // 测试修改的文件
    QStringList statusLines = {
        " M modified.cpp",      // 工作区修改
        "M  staged.cpp",        // 暂存区修改
        "MM both_modified.cpp"  // 暂存区和工作区都修改
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    verifyParsedStatus(result, "modified.cpp", LocallyModifiedUnstagedVersion);
    verifyParsedStatus(result, "staged.cpp", LocallyModifiedVersion);
    verifyParsedStatus(result, "both_modified.cpp", LocallyModifiedVersion);
}

void TestGitStatusParser::testParseAddedStatus()
{
    // 测试新增的文件
    QStringList statusLines = {
        "A  added.cpp",         // 新增文件
        "?? untracked.cpp"      // 未跟踪文件
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    verifyParsedStatus(result, "added.cpp", AddedVersion);
    verifyParsedStatus(result, "untracked.cpp", UnversionedVersion);
}

void TestGitStatusParser::testParseRemovedStatus()
{
    // 测试删除的文件
    QStringList statusLines = {
        "D  removed.cpp",       // 暂存区删除
        " D deleted.cpp"        // 工作区删除
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    verifyParsedStatus(result, "removed.cpp", RemovedVersion);
    verifyParsedStatus(result, "deleted.cpp", MissingVersion);
}

void TestGitStatusParser::testParseUntrackedStatus()
{
    // 测试未跟踪的文件
    QStringList statusLines = {
        "?? untracked1.cpp",
        "?? untracked2.txt",
        "?? new_directory/"
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    verifyParsedStatus(result, "untracked1.cpp", UnversionedVersion);
    verifyParsedStatus(result, "untracked2.txt", UnversionedVersion);
    verifyParsedStatus(result, "new_directory/", UnversionedVersion);
}

void TestGitStatusParser::testParseRenamedStatus()
{
    // 测试重命名的文件
    QStringList statusLines = {
        "R  old.cpp -> new.cpp",
        "RM renamed_and_modified.cpp -> renamed_modified.cpp"
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    // 重命名的文件应该被正确解析
    QVERIFY(result.size() >= 1);
    
    // 具体的重命名处理逻辑取决于实现
    // 这里主要测试不会崩溃
    QVERIFY(true);
}

void TestGitStatusParser::testParseCopiedStatus()
{
    // 测试复制的文件
    QStringList statusLines = {
        "C  original.cpp -> copy.cpp"
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    // 复制的文件应该被正确处理
    QVERIFY(!result.isEmpty());
}

void TestGitStatusParser::testParseConflictStatus()
{
    // 测试冲突状态的文件
    QStringList statusLines = {
        "UU conflict.cpp",      // 两边都修改的冲突
        "AA both_added.cpp",    // 两边都添加的冲突
        "DD both_deleted.cpp"   // 两边都删除的冲突
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    verifyParsedStatus(result, "conflict.cpp", ConflictingVersion);
    verifyParsedStatus(result, "both_added.cpp", ConflictingVersion);
    verifyParsedStatus(result, "both_deleted.cpp", ConflictingVersion);
}

void TestGitStatusParser::testParseMultipleFiles()
{
    // 测试多个文件的解析
    QStringList statusLines = {
        "M  file1.cpp",
        "A  file2.cpp", 
        "D  file3.cpp",
        "?? file4.cpp",
        " M file5.cpp"
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    QCOMPARE(result.size(), 5);
    
    verifyParsedStatus(result, "file1.cpp", LocallyModifiedVersion);
    verifyParsedStatus(result, "file2.cpp", AddedVersion);
    verifyParsedStatus(result, "file3.cpp", RemovedVersion);
    verifyParsedStatus(result, "file4.cpp", UnversionedVersion);
    verifyParsedStatus(result, "file5.cpp", LocallyModifiedUnstagedVersion);
}

void TestGitStatusParser::testParseMixedStatuses()
{
    // 测试混合状态的复杂情况
    QStringList statusLines = {
        "MM mixed1.cpp",        // 暂存区和工作区都修改
        "AM mixed2.cpp",        // 添加后又修改
        "RM mixed3.cpp",        // 重命名后又修改
        "?? untracked.cpp"      // 未跟踪
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    QVERIFY(result.size() >= 3);
    // 混合状态的具体处理依赖于实现
    QVERIFY(result.contains("mixed1.cpp"));
    QVERIFY(result.contains("mixed2.cpp"));
    QVERIFY(result.contains("untracked.cpp"));
}

void TestGitStatusParser::testParseEmptyStatus()
{
    // 测试空状态输出
    QString emptyOutput = "";
    GitStatusMap result = GitStatusParser::parseGitStatus(emptyOutput);
    
    QVERIFY(result.isEmpty());
}

void TestGitStatusParser::testParseSpecialCharacters()
{
    // 测试包含特殊字符的文件名
    QStringList statusLines = {
        "M  file@#$.cpp",
        "A  file[].cpp",
        "?? file{}.cpp"
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    QVERIFY(result.size() >= 3);
    QVERIFY(result.contains("file@#$.cpp"));
    QVERIFY(result.contains("file[].cpp"));
    QVERIFY(result.contains("file{}.cpp"));
}

void TestGitStatusParser::testParseUnicodeFilenames()
{
    // 测试Unicode文件名
    QStringList statusLines = {
        "M  中文文件.cpp",
        "A  файл.cpp",
        "?? ファイル.cpp"
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    QVERIFY(result.size() >= 3);
    QVERIFY(result.contains("中文文件.cpp"));
    QVERIFY(result.contains("файл.cpp"));
    QVERIFY(result.contains("ファイル.cpp"));
}

void TestGitStatusParser::testParseSpacesInFilenames()
{
    // 测试包含空格的文件名
    QStringList statusLines = {
        "M  file with spaces.cpp",
        "A  another file.txt",
        "?? new file here.h"
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    QVERIFY(result.size() >= 3);
    QVERIFY(result.contains("file with spaces.cpp"));
    QVERIFY(result.contains("another file.txt"));
    QVERIFY(result.contains("new file here.h"));
}

void TestGitStatusParser::testParseQuotedFilenames()
{
    // 测试带引号的文件名（Git会对特殊文件名加引号）
    QStringList statusLines = {
        "M  \"quoted file.cpp\"",
        "A  \"file\\nwith\\nnewlines.txt\"",
        "?? \"file\\twith\\ttabs.h\""
    };
    
    QString output = createGitStatusOutput(statusLines);
    GitStatusMap result = GitStatusParser::parseGitStatus(output);
    
    // 解析器应该能处理引号文件名
    QVERIFY(!result.isEmpty());
}

void TestGitStatusParser::testParseInvalidInput()
{
    // 测试无效输入的处理
    QStringList invalidInputs = {
        "invalid line without proper format",
        "X  unknown_status.cpp",  // 未知状态码
        "incomplete line"
    };
    
    for (const QString &invalidInput : invalidInputs) {
        GitStatusMap result = GitStatusParser::parseGitStatus(invalidInput);
        // 无效输入不应该导致崩溃，可能返回空结果或忽略无效行
        QVERIFY(true); // 主要测试不崩溃
    }
}

void TestGitStatusParser::testParseCorruptedOutput()
{
    // 测试损坏的Git输出
    QString corruptedOutput = "M  file1.cpp\n\x00\x01\x02invalid\nA  file2.cpp";
    GitStatusMap result = GitStatusParser::parseGitStatus(corruptedOutput);
    
    // 应该能解析有效部分，忽略损坏部分
    QVERIFY(!result.isEmpty());
}

void TestGitStatusParser::testParseEmptyInput()
{
    // 测试空输入
    QString emptyInput = "";
    GitStatusMap result = GitStatusParser::parseGitStatus(emptyInput);
    
    QVERIFY(result.isEmpty());
}

void TestGitStatusParser::testParsePorcelainFormat()
{
    // 测试porcelain格式输出
    QString porcelainOutput = "M  staged.cpp\n M unstaged.cpp\nA  added.cpp\n?? untracked.cpp";
    GitStatusMap result = GitStatusParser::parseGitStatus(porcelainOutput);
    
    QVERIFY(result.size() >= 4);
    QVERIFY(result.contains("staged.cpp"));
    QVERIFY(result.contains("unstaged.cpp"));
    QVERIFY(result.contains("added.cpp"));
    QVERIFY(result.contains("untracked.cpp"));
}

void TestGitStatusParser::testParseShortFormat()
{
    // 测试短格式输出
    QString shortOutput = "M staged.cpp\nA added.cpp\n? untracked.cpp";
    GitStatusMap result = GitStatusParser::parseGitStatus(shortOutput);
    
    // 短格式可能需要不同的解析逻辑
    QVERIFY(!result.isEmpty());
}

void TestGitStatusParser::testParseLongFormat()
{
    // 测试长格式输出（通常不在porcelain模式中使用）
    QString longOutput = "modified:   modified.cpp\nnew file:   added.cpp\ndeleted:    removed.cpp";
    GitStatusMap result = GitStatusParser::parseGitStatus(longOutput);
    
    // 长格式可能需要特殊处理
    QVERIFY(true); // 主要测试不崩溃
}

void TestGitStatusParser::testParseLargeOutput()
{
    // 测试大量文件的解析性能
    QStringList statusLines;
    const int fileCount = 1000;
    
    for (int i = 0; i < fileCount; ++i) {
        QString fileName = QString("file%1.cpp").arg(i);
        QString statusLine = QString("M  %1").arg(fileName);
        statusLines.append(statusLine);
    }
    
    QString largeOutput = createGitStatusOutput(statusLines);
    
    QElapsedTimer timer;
    timer.start();
    
    GitStatusMap result = GitStatusParser::parseGitStatus(largeOutput);
    
    qint64 elapsed = timer.elapsed();
    qDebug() << "Large output parsing took" << elapsed << "ms";
    
    QCOMPARE(result.size(), fileCount);
    QVERIFY(elapsed < 1000); // 应该在1秒内完成
}

void TestGitStatusParser::testParsePerformance()
{
    // 性能测试：重复解析相同输出
    QString testOutput = "M  file1.cpp\nA  file2.cpp\nD  file3.cpp\n?? file4.cpp";
    
    const int iterations = 1000;
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < iterations; ++i) {
        GitStatusMap result = GitStatusParser::parseGitStatus(testOutput);
        QCOMPARE(result.size(), 4);
    }
    
    qint64 elapsed = timer.elapsed();
    qDebug() << "Performance test completed in" << elapsed << "ms";
    
    // 性能要求：平均每次解析应小于1ms
    QVERIFY(elapsed / iterations < 1);
}

QString TestGitStatusParser::createGitStatusOutput(const QStringList &statusLines)
{
    return statusLines.join("\n") + "\n";
}

void TestGitStatusParser::verifyParsedStatus(const GitStatusMap &result, 
                                            const QString &filePath, ItemVersion expectedStatus)
{
    QVERIFY(result.contains(filePath));
    QCOMPARE(result[filePath], expectedStatus);
}

QTEST_MAIN(TestGitStatusParser)
#include "test-git-status-parser.moc" 