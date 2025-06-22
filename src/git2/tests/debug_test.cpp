#include <QtTest/QtTest>
#include "utils/test-utils.h"

class DebugTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testMockRepo()
    {
        qDebug() << "=== Starting debug test ===";
        
        MockGitRepository* repo = new MockGitRepository();
        qDebug() << "MockGitRepository created";
        qDebug() << "Repository path:" << repo->repositoryPath();
        
        qDebug() << "Calling initialize()...";
        bool result = repo->initialize();
        qDebug() << "Initialize result:" << result;
        
        delete repo;
        qDebug() << "=== Debug test completed ===";
        
        QVERIFY(result);
    }
};

QTEST_MAIN(DebugTest)
#include "debug_test.moc" 