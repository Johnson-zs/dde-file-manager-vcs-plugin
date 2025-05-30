#ifndef GITCHECKOUTDIALOG_H
#define GITCHECKOUTDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>

/**
 * @brief Git Checkout对话框
 */
class GitCheckoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitCheckoutDialog(const QString &repositoryPath, QWidget *parent = nullptr);

private Q_SLOTS:
    void onCheckoutClicked();
    void onCancelClicked();
    void onBranchDoubleClicked();

private:
    void setupUI();
    void loadBranches();
    void loadTags();

    QString m_repositoryPath;
    QListWidget *m_branchList;
    QListWidget *m_tagList;
    QLineEdit *m_newBranchEdit;
    QPushButton *m_checkoutButton;
    QPushButton *m_cancelButton;
};

#endif // GITCHECKOUTDIALOG_H 