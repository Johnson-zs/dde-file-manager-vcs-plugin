#ifndef CHARACTERANIMATIONWIDGET_H
#define CHARACTERANIMATIONWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QStringList>

/**
 * @brief 字符动画组件
 * 
 * 提供旋转字符动画效果，用于显示操作进行中的状态。
 * 可以在多个对话框中复用，避免代码重复。
 */
class CharacterAnimationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CharacterAnimationWidget(QWidget *parent = nullptr);
    ~CharacterAnimationWidget();

    /**
     * @brief 开始动画
     * @param baseText 基础文本，动画字符会添加在前面
     */
    void startAnimation(const QString &baseText = QString());

    /**
     * @brief 停止动画
     */
    void stopAnimation();

    /**
     * @brief 设置动画间隔
     * @param interval 间隔时间（毫秒）
     */
    void setAnimationInterval(int interval);

    /**
     * @brief 设置基础文本
     * @param text 要显示的基础文本
     */
    void setBaseText(const QString &text);

    /**
     * @brief 设置文本样式
     * @param styleSheet CSS样式表
     */
    void setTextStyleSheet(const QString &styleSheet);

    /**
     * @brief 获取标签组件，用于进一步自定义
     * @return QLabel指针
     */
    QLabel* getLabel() const { return m_label; }

    /**
     * @brief 检查动画是否正在运行
     * @return true如果动画正在运行
     */
    bool isAnimating() const { return m_timer->isActive(); }

private slots:
    void onAnimationTimer();

private:
    void updateAnimationText();

    QLabel *m_label;
    QTimer *m_timer;
    QStringList m_animationFrames;
    QString m_baseText;
    int m_animationStep;
};

#endif // CHARACTERANIMATIONWIDGET_H 