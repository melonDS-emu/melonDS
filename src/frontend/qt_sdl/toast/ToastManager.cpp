#include "ToastManager.h"
#include "toast/ToastOverlay.h"

ToastManager::ToastManager(QObject* parent)
    : QObject(parent)
{
}

void ToastManager::Init(QWidget* widget)
{
    if (m_overlay) return;

    m_overlay = new ToastOverlay(widget);
    
    m_overlay->setGeometry(widget->rect());
    widget->installEventFilter(m_overlay);
    m_overlay->show();
    m_overlay->raise();
}
void ToastManager::ShowAchievement(const QString& title, const QString& description, const QPixmap& icon)
{
    if (!m_overlay) {
        return; 
    }
    m_overlay->ShowToast(title, description, icon);
}
void ToastManager::ShowChallengeIndicator(const QPixmap& icon)
{
    if (m_overlay) {
        m_overlay->ShowChallenge(icon);
    }
}

void ToastManager::HideChallengeIndicator()
{
    if (m_overlay) {
        m_overlay->HideChallenge();
    }
}
void ToastManager::ClearAll()
{
    if (m_overlay) {
        m_overlay->HideChallenge();
    }
}