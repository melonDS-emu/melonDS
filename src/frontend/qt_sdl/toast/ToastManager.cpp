#include "ToastManager.h"
#include "toast/ToastOverlay.h"
#include <QLabel>
#include "config.h"
#include "Window.h"
#include "EmuInstance.h"

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
void ToastManager::ShowLeaderboardToast(const QString& title, const QString& description, const QPixmap& icon, bool playSound)
{
    if (!m_overlay) return;
    m_overlay->ShowLeaderboardToast(title, description, icon, false);
}
void ToastManager::ShowProgress(const QString& title, const QString& description, const QPixmap& icon)
{
    if (!m_overlay) return;
    m_overlay->ShowToast(title, description, icon, false);
}

void ToastManager::ShowChallengeIndicator(const QPixmap& icon, const QString& badgeName)
{
    if (m_overlay)
        m_overlay->ShowChallenge(icon, badgeName);
}

void ToastManager::HideChallengeIndicator(const QString& badgeName)
{
    if (m_overlay)
        m_overlay->HideChallenge(badgeName);
}

void ToastManager::UpdateLeaderboardTracker(const QString& display)
{
    MainWindow* mainWin = qobject_cast<MainWindow*>(this->parent());
    
    if (mainWin) {
        if (mainWin->getEmuInstance()->getLocalConfig().GetBool("RetroAchievements.HideLeaderboardCounter")) {
            if (m_leaderboardLabel) m_leaderboardLabel->hide();
            return; 
        }
    }

    QWidget* parent = qobject_cast<QWidget*>(this->parent());
    if (!parent) return;

    if (display.isEmpty()) {
        if (m_leaderboardLabel) m_leaderboardLabel->hide();
        return;
    }

    if (!m_leaderboardLabel) {
        m_leaderboardLabel = new QLabel(parent); 
        m_leaderboardLabel->setStyleSheet(
            "background-color: rgba(0, 0, 0, 160);"
            "color: white;"
            "padding: 4px 8px;"
            "border-radius: 4px;"
            "font-family: 'Consolas', 'Monospace';"
            "font-weight: bold;"
            "font-size: 13px;"
        );
        m_leaderboardLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    m_leaderboardLabel->setText(display);
    m_leaderboardLabel->adjustSize();

    int x = parent->width() - m_leaderboardLabel->width() - 10;
    int y = parent->height() - m_leaderboardLabel->height() - 10;
    
    m_leaderboardLabel->move(x, y);
    m_leaderboardLabel->show();
    m_leaderboardLabel->raise();
}

void ToastManager::UpdateNetworkStatus(const QString& display)
{
    QWidget* parent = qobject_cast<QWidget*>(this->parent());
    if (!parent) return;

    if (display.isEmpty()) {
        if (m_networkLabel) m_networkLabel->hide();
        return;
    }

    if (!m_networkLabel) {
        m_networkLabel = new QLabel(parent); 
        m_networkLabel->setStyleSheet(
            "background-color: rgba(0, 0, 0, 180);"
            "color: white;"
            "padding: 4px 8px;"
            "font-family: 'Consolas', 'Monospace';"
            "font-weight: bold;"
        );
        m_networkLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    m_networkLabel->setText(display);
    m_networkLabel->adjustSize();

    int offset = (m_leaderboardLabel && m_leaderboardLabel->isVisible()) 
                 ? m_leaderboardLabel->height() + 15 : 10;

    int x = parent->width() - m_networkLabel->width() - 10;
    int y = parent->height() - m_networkLabel->height() - offset;
    
    m_networkLabel->move(x, y);
    m_networkLabel->show();
    m_networkLabel->raise();
}

void ToastManager::ClearAll()
{
    if (m_overlay)
        m_overlay->HideChallenge(""); 
}