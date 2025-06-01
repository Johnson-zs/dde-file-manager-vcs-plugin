#include "gitfilepreviewdialog.h"
#include "gitdialogs.h"
#include "widgets/linenumbertextedit.h"
#include "widgets/filerenderer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QMessageBox>
#include <QApplication>
#include <QFont>
#include <QKeyEvent>
#include <QMimeDatabase>
#include <QMimeType>
#include <QTextDocument>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QStackedWidget>
#include <QTextBrowser>

/**
 * @brief 简单的语法高亮器，支持常见文件类型
 */
class SimpleSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit SimpleSyntaxHighlighter(QTextDocument *parent = nullptr)
        : QSyntaxHighlighter(parent)
    {
        setupFormats();
    }

    void setFileType(const QString &fileType)
    {
        m_fileType = fileType.toLower();
        setupRules();
        rehighlight();
    }

protected:
    void highlightBlock(const QString &text) override
    {
        for (const auto &rule : m_highlightingRules) {
            QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
            while (matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void setupFormats()
    {
        // 关键字格式
        m_keywordFormat.setForeground(QColor(0, 0, 255));
        m_keywordFormat.setFontWeight(QFont::Bold);

        // 字符串格式
        m_stringFormat.setForeground(QColor(163, 21, 21));

        // 注释格式
        m_commentFormat.setForeground(QColor(0, 128, 0));
        m_commentFormat.setFontItalic(true);

        // 数字格式
        m_numberFormat.setForeground(QColor(255, 140, 0));

        // 函数格式
        m_functionFormat.setForeground(QColor(128, 0, 128));
        m_functionFormat.setFontWeight(QFont::Bold);
    }

    void setupRules()
    {
        m_highlightingRules.clear();

        if (m_fileType.contains("cpp") || m_fileType.contains("c++") || 
            m_fileType.contains("cc") || m_fileType.contains("cxx") ||
            m_fileType == "c" || m_fileType == "h" || m_fileType == "hpp") {
            setupCppRules();
        } else if (m_fileType.contains("python") || m_fileType == "py") {
            setupPythonRules();
        } else if (m_fileType.contains("javascript") || m_fileType == "js" || 
                   m_fileType == "ts" || m_fileType.contains("typescript")) {
            setupJavaScriptRules();
        } else if (m_fileType.contains("xml") || m_fileType.contains("html") || 
                   m_fileType.contains("htm")) {
            setupXmlRules();
        } else if (m_fileType.contains("json")) {
            setupJsonRules();
        } else if (m_fileType.contains("cmake")) {
            setupCMakeRules();
        } else if (m_fileType.contains("qml")) {
            setupQmlRules();
        }

        // 通用规则
        setupCommonRules();
    }

    void setupCppRules()
    {
        HighlightingRule rule;

        // C++ 关键字
        QStringList keywordPatterns = {
            "\\bclass\\b", "\\bstruct\\b", "\\benum\\b", "\\bunion\\b",
            "\\bpublic\\b", "\\bprivate\\b", "\\bprotected\\b",
            "\\bvirtual\\b", "\\boverride\\b", "\\bfinal\\b",
            "\\bconst\\b", "\\bstatic\\b", "\\bextern\\b", "\\binline\\b",
            "\\bvolatile\\b", "\\bmutable\\b", "\\bconstexpr\\b",
            "\\bif\\b", "\\belse\\b", "\\bfor\\b", "\\bwhile\\b", "\\bdo\\b",
            "\\bswitch\\b", "\\bcase\\b", "\\bdefault\\b", "\\bbreak\\b", "\\bcontinue\\b",
            "\\breturn\\b", "\\btry\\b", "\\bcatch\\b", "\\bthrow\\b",
            "\\bnamespace\\b", "\\busing\\b", "\\btypedef\\b", "\\btypename\\b",
            "\\btemplate\\b", "\\bauto\\b", "\\bdecltype\\b"
        };

        for (const QString &pattern : keywordPatterns) {
            rule.pattern = QRegularExpression(pattern);
            rule.format = m_keywordFormat;
            m_highlightingRules.append(rule);
        }

        // 函数调用
        rule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()");
        rule.format = m_functionFormat;
        m_highlightingRules.append(rule);
    }

    void setupPythonRules()
    {
        HighlightingRule rule;

        // Python 关键字
        QStringList keywordPatterns = {
            "\\bdef\\b", "\\bclass\\b", "\\bif\\b", "\\belse\\b", "\\belif\\b",
            "\\bfor\\b", "\\bwhile\\b", "\\btry\\b", "\\bexcept\\b", "\\bfinally\\b",
            "\\bwith\\b", "\\bas\\b", "\\bimport\\b", "\\bfrom\\b", "\\breturn\\b",
            "\\byield\\b", "\\blambda\\b", "\\band\\b", "\\bor\\b", "\\bnot\\b",
            "\\bin\\b", "\\bis\\b", "\\bTrue\\b", "\\bFalse\\b", "\\bNone\\b"
        };

        for (const QString &pattern : keywordPatterns) {
            rule.pattern = QRegularExpression(pattern);
            rule.format = m_keywordFormat;
            m_highlightingRules.append(rule);
        }
    }

    void setupJavaScriptRules()
    {
        HighlightingRule rule;

        // JavaScript 关键字
        QStringList keywordPatterns = {
            "\\bfunction\\b", "\\bvar\\b", "\\blet\\b", "\\bconst\\b",
            "\\bif\\b", "\\belse\\b", "\\bfor\\b", "\\bwhile\\b", "\\bdo\\b",
            "\\bswitch\\b", "\\bcase\\b", "\\bdefault\\b", "\\bbreak\\b", "\\bcontinue\\b",
            "\\breturn\\b", "\\btry\\b", "\\bcatch\\b", "\\bfinally\\b", "\\bthrow\\b",
            "\\btrue\\b", "\\bfalse\\b", "\\bnull\\b", "\\bundefined\\b",
            "\\bclass\\b", "\\bextends\\b", "\\bsuper\\b", "\\bthis\\b"
        };

        for (const QString &pattern : keywordPatterns) {
            rule.pattern = QRegularExpression(pattern);
            rule.format = m_keywordFormat;
            m_highlightingRules.append(rule);
        }
    }

    void setupXmlRules()
    {
        HighlightingRule rule;

        // XML 标签
        rule.pattern = QRegularExpression("<[!?/]?\\b[A-Za-z_][A-Za-z0-9_-]*(?:\\s|>|/>)");
        rule.format = m_keywordFormat;
        m_highlightingRules.append(rule);

        // XML 属性
        rule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_-]*(?=\\s*=)");
        rule.format = m_functionFormat;
        m_highlightingRules.append(rule);
    }

    void setupJsonRules()
    {
        HighlightingRule rule;

        // JSON 键
        rule.pattern = QRegularExpression("\"[^\"]*\"(?=\\s*:)");
        rule.format = m_keywordFormat;
        m_highlightingRules.append(rule);

        // JSON 值（布尔和null）
        rule.pattern = QRegularExpression("\\b(true|false|null)\\b");
        rule.format = m_functionFormat;
        m_highlightingRules.append(rule);
    }

    void setupCMakeRules()
    {
        HighlightingRule rule;

        // CMake 命令（函数）
        QStringList commandPatterns = {
            "\\bcmake_minimum_required\\b", "\\bproject\\b", "\\badd_executable\\b", "\\badd_library\\b",
            "\\btarget_link_libraries\\b", "\\btarget_include_directories\\b", "\\btarget_compile_definitions\\b",
            "\\btarget_compile_options\\b", "\\bfind_package\\b", "\\bfind_library\\b", "\\bfind_path\\b",
            "\\binclude_directories\\b", "\\blink_directories\\b", "\\bset\\b", "\\bunset\\b",
            "\\blist\\b", "\\bstring\\b", "\\bmath\\b", "\\bfile\\b", "\\bget_filename_component\\b",
            "\\bif\\b", "\\belse\\b", "\\belseif\\b", "\\bendif\\b", "\\bforeach\\b", "\\bendforeach\\b",
            "\\bwhile\\b", "\\bendwhile\\b", "\\bfunction\\b", "\\bendfunction\\b", "\\bmacro\\b", "\\bendmacro\\b",
            "\\binclude\\b", "\\badd_subdirectory\\b", "\\boption\\b", "\\bconfigure_file\\b",
            "\\binstall\\b", "\\bmessage\\b", "\\breturn\\b", "\\bbreak\\b", "\\bcontinue\\b"
        };

        for (const QString &pattern : commandPatterns) {
            rule.pattern = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
            rule.format = m_keywordFormat;
            m_highlightingRules.append(rule);
        }

        // CMake 变量引用 ${VAR}
        rule.pattern = QRegularExpression("\\$\\{[^}]+\\}");
        rule.format = m_functionFormat;
        m_highlightingRules.append(rule);

        // CMake 生成器表达式 $<...>
        rule.pattern = QRegularExpression("\\$<[^>]+>");
        rule.format = m_functionFormat;
        m_highlightingRules.append(rule);

        // CMake 属性和目标
        rule.pattern = QRegularExpression("\\b[A-Z_][A-Z0-9_]*\\b");
        rule.format = m_numberFormat;
        m_highlightingRules.append(rule);
    }

    void setupQmlRules()
    {
        HighlightingRule rule;

        // QML 基本类型和关键字
        QStringList keywordPatterns = {
            "\\bimport\\b", "\\bas\\b", "\\bproperty\\b", "\\balias\\b", "\\bsignal\\b", "\\bfunction\\b",
            "\\bif\\b", "\\belse\\b", "\\bfor\\b", "\\bwhile\\b", "\\bdo\\b", "\\bswitch\\b", "\\bcase\\b",
            "\\bdefault\\b", "\\bbreak\\b", "\\bcontinue\\b", "\\breturn\\b", "\\btry\\b", "\\bcatch\\b",
            "\\bfinally\\b", "\\bthrow\\b", "\\bvar\\b", "\\blet\\b", "\\bconst\\b", "\\btrue\\b", "\\bfalse\\b",
            "\\bnull\\b", "\\bundefined\\b", "\\bthis\\b", "\\broot\\b", "\\bparent\\b"
        };

        for (const QString &pattern : keywordPatterns) {
            rule.pattern = QRegularExpression(pattern);
            rule.format = m_keywordFormat;
            m_highlightingRules.append(rule);
        }

        // QML 基本类型
        QStringList typePatterns = {
            "\\bItem\\b", "\\bRectangle\\b", "\\bText\\b", "\\bImage\\b", "\\bMouseArea\\b",
            "\\bColumn\\b", "\\bRow\\b", "\\bGrid\\b", "\\bFlow\\b", "\\bRepeater\\b",
            "\\bListView\\b", "\\bGridView\\b", "\\bPathView\\b", "\\bScrollView\\b",
            "\\bStackView\\b", "\\bLoader\\b", "\\bComponent\\b", "\\bConnections\\b",
            "\\bTimer\\b", "\\bAnimation\\b", "\\bBehavior\\b", "\\bTransition\\b",
            "\\bState\\b", "\\bStateGroup\\b", "\\bPropertyChanges\\b", "\\bAnchorChanges\\b",
            "\\bint\\b", "\\breal\\b", "\\bdouble\\b", "\\bbool\\b", "\\bstring\\b", "\\bcolor\\b",
            "\\bdate\\b", "\\burl\\b", "\\bvar\\b", "\\bvariant\\b", "\\blist\\b"
        };

        for (const QString &pattern : typePatterns) {
            rule.pattern = QRegularExpression(pattern);
            rule.format = m_functionFormat;
            m_highlightingRules.append(rule);
        }

        // QML 属性绑定
        rule.pattern = QRegularExpression("\\b[a-zA-Z_][a-zA-Z0-9_]*(?=\\s*:)");
        rule.format = m_numberFormat;
        m_highlightingRules.append(rule);

        // QML ID 引用
        rule.pattern = QRegularExpression("\\bid\\s*:\\s*[a-zA-Z_][a-zA-Z0-9_]*");
        rule.format = m_functionFormat;
        m_highlightingRules.append(rule);
    }

    void setupCommonRules()
    {
        HighlightingRule rule;

        // 字符串（双引号）
        rule.pattern = QRegularExpression("\".*\"");
        rule.format = m_stringFormat;
        m_highlightingRules.append(rule);

        // 字符串（单引号）
        rule.pattern = QRegularExpression("'.*'");
        rule.format = m_stringFormat;
        m_highlightingRules.append(rule);

        // 数字
        rule.pattern = QRegularExpression("\\b\\d+(\\.\\d+)?\\b");
        rule.format = m_numberFormat;
        m_highlightingRules.append(rule);

        // 单行注释 //
        rule.pattern = QRegularExpression("//[^\n]*");
        rule.format = m_commentFormat;
        m_highlightingRules.append(rule);

        // 单行注释 #
        rule.pattern = QRegularExpression("#[^\n]*");
        rule.format = m_commentFormat;
        m_highlightingRules.append(rule);

        // 多行注释 /* */
        rule.pattern = QRegularExpression("/\\*.*\\*/");
        rule.format = m_commentFormat;
        m_highlightingRules.append(rule);
    }

    QString m_fileType;
    QVector<HighlightingRule> m_highlightingRules;

    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_functionFormat;
};

// ============================================================================
// GitFilePreviewDialog Implementation
// ============================================================================

GitFilePreviewDialog::GitFilePreviewDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
    : QDialog(parent)
    , m_repositoryPath(repositoryPath)
    , m_filePath(filePath)
    , m_isCommitMode(false)
    , m_fileInfoLabel(nullptr)
    , m_textEdit(nullptr)
    , m_specialRendererWidget(nullptr)
    , m_openFileButton(nullptr)
    , m_showInFolderButton(nullptr)
    , m_toggleViewButton(nullptr)
    , m_closeButton(nullptr)
    , m_specialRenderer(nullptr)
    , m_usingSpecialRenderer(false)
{
    qInfo() << "INFO: [GitFilePreviewDialog] Initializing file preview for:" << filePath;
    
    setupUI();
    loadFileContent();
    setupSpecialRenderer();
    setupSyntaxHighlighter();
    
    qInfo() << "INFO: [GitFilePreviewDialog] File preview dialog initialized successfully";
}

GitFilePreviewDialog::GitFilePreviewDialog(const QString &repositoryPath, const QString &filePath, 
                                           const QString &commitHash, QWidget *parent)
    : QDialog(parent)
    , m_repositoryPath(repositoryPath)
    , m_filePath(filePath)
    , m_commitHash(commitHash)
    , m_isCommitMode(true)
    , m_fileInfoLabel(nullptr)
    , m_textEdit(nullptr)
    , m_specialRendererWidget(nullptr)
    , m_openFileButton(nullptr)
    , m_showInFolderButton(nullptr)
    , m_toggleViewButton(nullptr)
    , m_closeButton(nullptr)
    , m_specialRenderer(nullptr)
    , m_usingSpecialRenderer(false)
{
    qInfo() << "INFO: [GitFilePreviewDialog] Initializing file preview for:" << filePath 
            << "at commit:" << commitHash.left(8);
    
    setupUI();
    loadFileContentAtCommit();
    setupSpecialRenderer();
    setupSyntaxHighlighter();
    
    qInfo() << "INFO: [GitFilePreviewDialog] File preview dialog (commit mode) initialized successfully";
}

GitFilePreviewDialog::~GitFilePreviewDialog()
{
    delete m_specialRenderer;
    m_specialRenderer = nullptr;
}

GitFilePreviewDialog* GitFilePreviewDialog::showFilePreview(const QString &repositoryPath, 
                                                            const QString &filePath, QWidget *parent)
{
    auto *dialog = new GitFilePreviewDialog(repositoryPath, filePath, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    return dialog;
}

GitFilePreviewDialog* GitFilePreviewDialog::showFilePreviewAtCommit(const QString &repositoryPath, 
                                                                    const QString &filePath, 
                                                                    const QString &commitHash, QWidget *parent)
{
    auto *dialog = new GitFilePreviewDialog(repositoryPath, filePath, commitHash, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    return dialog;
}

void GitFilePreviewDialog::setupUI()
{
    QString fileName = QFileInfo(m_filePath).fileName();
    
    if (m_isCommitMode) {
        setWindowTitle(tr("File Preview - %1 at %2").arg(fileName, m_commitHash.left(8)));
    } else {
        setWindowTitle(tr("File Preview - %1").arg(fileName));
    }
    
    setModal(false);
    setMinimumSize(600, 400);
    resize(900, 700);
    setAttribute(Qt::WA_DeleteOnClose);
    
    // 设置窗口图标
    setWindowIcon(QIcon::fromTheme("document-preview"));
    
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    
    // === 文件信息区域 ===
    m_fileInfoLabel = new QLabel(this);
    m_fileInfoLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 8px;"
        "    font-weight: bold;"
        "    color: #495057;"
        "}"
    );
    
    QString infoText;
    if (m_isCommitMode) {
        infoText = tr("File: %1\nCommit: %2\nPress Space to close").arg(m_filePath, m_commitHash);
    } else {
        infoText = tr("File: %1\nPress Space to close").arg(m_filePath);
    }
    
    // 检查是否有特殊渲染器
    if (FileRendererFactory::hasRenderer(m_filePath)) {
        infoText += tr("\nSpecial renderer available - Toggle view with button below");
        m_usingSpecialRenderer = true;
    }
    
    m_fileInfoLabel->setText(infoText);
    mainLayout->addWidget(m_fileInfoLabel);
    
    // === 文本编辑区域 ===
    m_textEdit = new LineNumberTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setFont(QFont("Consolas", 10));
    m_textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_textEdit->setPlaceholderText(tr("Loading file content..."));
    
    // 设置样式
    m_textEdit->setStyleSheet(
        "LineNumberTextEdit {"
        "    background-color: #ffffff;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    selection-background-color: #007acc;"
        "    selection-color: white;"
        "}"
    );
    
    if (m_usingSpecialRenderer) {
        // 如果有特殊渲染器，先隐藏文本编辑器
        m_textEdit->hide();
    }
    
    mainLayout->addWidget(m_textEdit);
    
    // === 按钮区域 ===
    auto *buttonLayout = new QHBoxLayout();
    
    // 只在非commit模式下显示文件操作按钮
    if (!m_isCommitMode) {
        m_openFileButton = new QPushButton(tr("Open File"), this);
        m_openFileButton->setIcon(QIcon::fromTheme("document-open"));
        m_openFileButton->setToolTip(tr("Open file with default application"));
        buttonLayout->addWidget(m_openFileButton);
        
        m_showInFolderButton = new QPushButton(tr("Show in Folder"), this);
        m_showInFolderButton->setIcon(QIcon::fromTheme("folder-open"));
        m_showInFolderButton->setToolTip(tr("Show file in file manager"));
        buttonLayout->addWidget(m_showInFolderButton);
        
        connect(m_openFileButton, &QPushButton::clicked, this, &GitFilePreviewDialog::onOpenFileClicked);
        connect(m_showInFolderButton, &QPushButton::clicked, this, &GitFilePreviewDialog::onShowInFolderClicked);
    }
    
    // 特殊渲染器模式切换按钮
    if (m_usingSpecialRenderer) {
        m_toggleViewButton = new QPushButton(tr("Show Source"), this);
        m_toggleViewButton->setIcon(QIcon::fromTheme("text-x-generic"));
        m_toggleViewButton->setToolTip(tr("Toggle between rendered view and source code"));
        buttonLayout->addWidget(m_toggleViewButton);
        
        connect(m_toggleViewButton, &QPushButton::clicked, this, &GitFilePreviewDialog::onToggleViewModeClicked);
    }
    
    buttonLayout->addStretch();
    
    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setIcon(QIcon::fromTheme("window-close"));
    m_closeButton->setDefault(true);
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // === 信号连接 ===
    connect(m_closeButton, &QPushButton::clicked, this, &GitFilePreviewDialog::onCloseClicked);
    
    // 设置焦点到文本编辑器，以便接收键盘事件
    m_textEdit->setFocus();
}

void GitFilePreviewDialog::loadFileContent()
{
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(m_filePath);
    QFileInfo fileInfo(absolutePath);
    
    if (!fileInfo.exists()) {
        QString errorMsg = tr("File not found: %1").arg(absolutePath);
        m_fileContent = errorMsg;
        m_textEdit->setPlainText(errorMsg);
        qWarning() << "WARNING: [GitFilePreviewDialog] File not found:" << absolutePath;
        return;
    }
    
    if (!fileInfo.isFile()) {
        QString errorMsg = tr("Path is not a file: %1").arg(absolutePath);
        m_fileContent = errorMsg;
        m_textEdit->setPlainText(errorMsg);
        qWarning() << "WARNING: [GitFilePreviewDialog] Path is not a file:" << absolutePath;
        return;
    }
    
    // 检查文件大小，避免加载过大的文件
    const qint64 maxFileSize = 10 * 1024 * 1024; // 10MB
    if (fileInfo.size() > maxFileSize) {
        QString errorMsg = tr("File is too large to preview (size: %1 MB)\n\n"
                              "Maximum preview size: 10 MB\n"
                              "Use 'Open File' button to view with external application.")
                           .arg(fileInfo.size() / (1024.0 * 1024.0), 0, 'f', 2);
        m_fileContent = errorMsg;
        m_textEdit->setPlainText(errorMsg);
        return;
    }
    
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString errorMsg = tr("Failed to open file: %1\nError: %2")
                           .arg(absolutePath, file.errorString());
        m_fileContent = errorMsg;
        m_textEdit->setPlainText(errorMsg);
        qWarning() << "WARNING: [GitFilePreviewDialog] Failed to open file:" << absolutePath 
                   << "Error:" << file.errorString();
        return;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString content = stream.readAll();
    
    if (content.isEmpty()) {
        m_fileContent = tr("File is empty or could not be read as text.");
        m_textEdit->setPlainText(m_fileContent);
    } else {
        m_fileContent = content;
        m_textEdit->setPlainText(content);
        qDebug() << "[GitFilePreviewDialog] Successfully loaded file content, size:" << content.length() << "characters";
    }
}

void GitFilePreviewDialog::loadFileContentAtCommit()
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    QStringList args;
    args << "show" << QString("%1:%2").arg(m_commitHash, m_filePath);
    
    qDebug() << "[GitFilePreviewDialog] Loading file content with git command:" << args;
    
    process.start("git", args);
    if (!process.waitForFinished(10000)) {
        QString errorMsg = tr("Failed to load file content from Git: %1\nError: %2")
                           .arg(m_filePath, process.errorString());
        m_fileContent = errorMsg;
        m_textEdit->setPlainText(errorMsg);
        qWarning() << "WARNING: [GitFilePreviewDialog] Git command failed:" << process.errorString();
        return;
    }
    
    if (process.exitCode() != 0) {
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());
        QString errorMsg = tr("Git command failed: %1\nError output: %2")
                           .arg(m_filePath, errorOutput);
        m_fileContent = errorMsg;
        m_textEdit->setPlainText(errorMsg);
        qWarning() << "WARNING: [GitFilePreviewDialog] Git command exit code:" << process.exitCode()
                   << "Error:" << errorOutput;
        return;
    }
    
    QString content = QString::fromUtf8(process.readAllStandardOutput());
    
    if (content.isEmpty()) {
        m_fileContent = tr("File is empty or could not be read from commit %1")
                        .arg(m_commitHash.left(8));
        m_textEdit->setPlainText(m_fileContent);
    } else {
        m_fileContent = content;
        m_textEdit->setPlainText(content);
        qDebug() << "[GitFilePreviewDialog] Successfully loaded file content from commit, size:" 
                 << content.length() << "characters";
    }
}

void GitFilePreviewDialog::setupSyntaxHighlighter()
{
    QString fileType = detectFileType();
    if (!fileType.isEmpty()) {
        m_syntaxHighlighter = std::make_unique<SimpleSyntaxHighlighter>(m_textEdit->document());
        static_cast<SimpleSyntaxHighlighter*>(m_syntaxHighlighter.get())->setFileType(fileType);
        qDebug() << "[GitFilePreviewDialog] Applied syntax highlighting for file type:" << fileType;
    }
}

QString GitFilePreviewDialog::detectFileType() const
{
    QFileInfo fileInfo(m_filePath);
    QString suffix = fileInfo.suffix().toLower();
    QString fileName = fileInfo.fileName().toLower();
    
    // 根据文件扩展名检测类型
    if (suffix == "cpp" || suffix == "cxx" || suffix == "cc" || suffix == "c++" ||
        suffix == "c" || suffix == "h" || suffix == "hpp" || suffix == "hxx") {
        return "cpp";
    } else if (suffix == "py" || suffix == "pyw") {
        return "python";
    } else if (suffix == "js" || suffix == "jsx") {
        return "javascript";
    } else if (suffix == "ts" || suffix == "tsx") {
        return "typescript";
    } else if (suffix == "html" || suffix == "htm") {
        return "html";
    } else if (suffix == "xml" || suffix == "xsl" || suffix == "xsd") {
        return "xml";
    } else if (suffix == "json") {
        return "json";
    } else if (suffix == "css") {
        return "css";
    } else if (suffix == "java") {
        return "java";
    } else if (suffix == "php") {
        return "php";
    } else if (suffix == "sh" || suffix == "bash") {
        return "shell";
    } else if (suffix == "qml" || suffix == "qmldir") {
        return "qml";
    } else if (suffix == "cmake" || fileName == "cmakelists.txt" || fileName.startsWith("cmake")) {
        return "cmake";
    } else if (suffix == "md" || suffix == "markdown" || suffix == "mdown" || 
               fileName == "readme" || fileName == "readme.md" || fileName == "readme.txt") {
        return "markdown";
    }
    
    return QString(); // 未知类型，不应用语法高亮
}

bool GitFilePreviewDialog::isMarkdownFile() const
{
    QFileInfo fileInfo(m_filePath);
    QString suffix = fileInfo.suffix().toLower();
    QString fileName = fileInfo.fileName().toLower();

    return (suffix == "md" || suffix == "markdown" || suffix == "mdown" ||
            fileName == "readme" || fileName == "readme.md" || fileName == "readme.txt");
}

void GitFilePreviewDialog::setupSpecialRenderer()
{
    if (!m_usingSpecialRenderer || m_fileContent.isEmpty()) {
        return;
    }
    
    // 创建特殊渲染器
    auto renderer = FileRendererFactory::createRenderer(m_filePath);
    if (!renderer) {
        qWarning() << "WARNING: [GitFilePreviewDialog] Failed to create special renderer for:" << m_filePath;
        m_usingSpecialRenderer = false;
        return;
    }
    
    m_specialRenderer = renderer.release(); // 转移所有权到原始指针
    
    // 创建渲染器 Widget
    m_specialRendererWidget = m_specialRenderer->createWidget(this);
    if (!m_specialRendererWidget) {
        qWarning() << "WARNING: [GitFilePreviewDialog] Failed to create special renderer widget";
        delete m_specialRenderer;
        m_specialRenderer = nullptr;
        m_usingSpecialRenderer = false;
        return;
    }
    
    // 设置内容
    m_specialRenderer->setContent(m_fileContent);
    
    // 添加到布局中（替换文本编辑器）
    auto *mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (mainLayout) {
        // 找到文本编辑器的位置并替换
        int textEditIndex = mainLayout->indexOf(m_textEdit);
        if (textEditIndex >= 0) {
            mainLayout->insertWidget(textEditIndex, m_specialRendererWidget);
            m_textEdit->hide();
        }
    }
    
    updateToggleButton();
    
    qInfo() << "INFO: [GitFilePreviewDialog] Special renderer setup completed for type:" 
            << m_specialRenderer->getRendererType();
}

void GitFilePreviewDialog::updateToggleButton()
{
    if (!m_toggleViewButton || !m_specialRenderer) {
        return;
    }
    
    if (m_specialRenderer->supportsViewToggle()) {
        m_toggleViewButton->setText(m_specialRenderer->getCurrentViewModeDescription());
        m_toggleViewButton->setVisible(true);
    } else {
        m_toggleViewButton->setVisible(false);
    }
}

void GitFilePreviewDialog::onToggleViewModeClicked()
{
    if (!m_specialRenderer || !m_usingSpecialRenderer) {
        return;
    }
    
    if (m_specialRenderer->supportsViewToggle()) {
        // 切换特殊渲染器的视图模式
        m_specialRenderer->toggleViewMode();
        updateToggleButton();
    } else {
        // 在特殊渲染器和源码视图之间切换
        if (m_specialRendererWidget->isVisible()) {
            m_specialRendererWidget->hide();
            m_textEdit->show();
            m_toggleViewButton->setText(tr("Show Rendered"));
        } else {
            m_textEdit->hide();
            m_specialRendererWidget->show();
            m_toggleViewButton->setText(tr("Show Source"));
        }
    }
    
    qDebug() << "[GitFilePreviewDialog] View mode toggled";
}

void GitFilePreviewDialog::onCloseClicked()
{
    close();
}

void GitFilePreviewDialog::onOpenFileClicked()
{
    if (m_isCommitMode) {
        return; // commit模式下不支持打开文件
    }
    
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(m_filePath);
    GitDialogManager::instance()->openFile(absolutePath, this);
}

void GitFilePreviewDialog::onShowInFolderClicked()
{
    if (m_isCommitMode) {
        return; // commit模式下不支持显示文件夹
    }
    
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(m_filePath);
    GitDialogManager::instance()->showFileInFolder(absolutePath, this);
}

void GitFilePreviewDialog::keyPressEvent(QKeyEvent *event)
{
    // 空格键关闭对话框
    if (event->key() == Qt::Key_Space) {
        close();
        return;
    }
    
    // Escape键也关闭对话框
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    
    QDialog::keyPressEvent(event);
}

#include "gitfilepreviewdialog.moc" 