#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    QCommandLineParser parser;
    parser.setApplicationDescription("DDE File Manager Git Dialog");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption actionOption(QStringList() << "a" << "action",
                                   "Git action to perform",
                                   "action");
    parser.addOption(actionOption);
    
    QCommandLineOption repositoryOption(QStringList() << "r" << "repository",
                                       "Git repository path",
                                       "repository");
    parser.addOption(repositoryOption);
    
    parser.process(app);
    
    QString action = parser.value(actionOption);
    QString repository = parser.value(repositoryOption);
    
    qDebug() << "Git Dialog action:" << action << "repository:" << repository;
    
    // TODO: 实现对话框启动逻辑
    
    return 0; // 暂时直接退出，避免显示空窗口
} 