#include "characteranimationwidget.h"

#include <QVBoxLayout>

CharacterAnimationWidget::CharacterAnimationWidget(QWidget *parent)
    : QWidget(parent), m_animationStep(0)
{
    // 初始化动画帧
    m_animationFrames << "⠋" << "⠙" << "⠹" << "⠸" << "⠼" << "⠴" << "⠦" << "⠧" << "⠇" << "⠏";

    // 创建标签
    m_label = new QLabel(this);
    m_label->setWordWrap(true);

    // 创建布局
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);

    // 创建定时器
    m_timer = new QTimer(this);
    m_timer->setInterval(100); // 默认100ms间隔
    connect(m_timer, &QTimer::timeout, this, &CharacterAnimationWidget::onAnimationTimer);

    qInfo() << "INFO: [CharacterAnimationWidget] Animation widget initialized";
}

CharacterAnimationWidget::~CharacterAnimationWidget()
{
    stopAnimation();
    qInfo() << "INFO: [CharacterAnimationWidget] Animation widget destroyed";
}

void CharacterAnimationWidget::startAnimation(const QString &baseText)
{
    if (!baseText.isEmpty()) {
        m_baseText = baseText;
    }

    m_animationStep = 0;
    updateAnimationText();
    m_timer->start();

    qInfo() << "INFO: [CharacterAnimationWidget::startAnimation] Animation started with text:" << m_baseText;
}

void CharacterAnimationWidget::stopAnimation()
{
    if (m_timer->isActive()) {
        m_timer->stop();
        qInfo() << "INFO: [CharacterAnimationWidget::stopAnimation] Animation stopped";
    }
}

void CharacterAnimationWidget::setAnimationInterval(int interval)
{
    m_timer->setInterval(interval);
}

void CharacterAnimationWidget::setBaseText(const QString &text)
{
    m_baseText = text;
    if (m_timer->isActive()) {
        updateAnimationText();
    } else {
        m_label->setText(text);
    }
}

void CharacterAnimationWidget::setTextStyleSheet(const QString &styleSheet)
{
    m_label->setStyleSheet(styleSheet);
}

void CharacterAnimationWidget::onAnimationTimer()
{
    updateAnimationText();
    m_animationStep = (m_animationStep + 1) % m_animationFrames.size();
}

void CharacterAnimationWidget::updateAnimationText()
{
    if (m_animationStep < m_animationFrames.size()) {
        QString animatedText = m_animationFrames[m_animationStep];
        if (!m_baseText.isEmpty()) {
            animatedText += " " + m_baseText;
        }
        m_label->setText(animatedText);
    }
} 