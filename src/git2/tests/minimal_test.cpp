#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDebug>

class MinimalTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testTempDir()
    {
        qDebug() << "Starting minimal test";
        
        QTemporaryDir tempDir;
        qDebug() << "QTemporaryDir created";
        qDebug() << "isValid():" << tempDir.isValid();
        qDebug() << "path():" << tempDir.path();
        qDebug() << "errorString():" << tempDir.errorString();
        
        QVERIFY(tempDir.isValid());
    }
};

QTEST_MAIN(MinimalTest)
#include "minimal_test.moc" 