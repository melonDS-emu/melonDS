#pragma once
#include <QObject>
#include <QLabel>

class ToastOverlay;

class ToastManager : public QObject
{
    Q_OBJECT
public:
    void UpdateNetworkStatus(const QString& display);
    void UpdateLeaderboardTracker(const QString& display);
    explicit ToastManager(QObject* parent = nullptr);

    void Init(QWidget* screenWidget);
    void ShowAchievement(const QString& title,
                         const QString& desc,
                         const QPixmap& icon);
    void ShowLeaderboardToast(const QString& title, const QString& description, const QPixmap& icon, bool playSound = true);
    void ShowProgress(const QString& title, const QString& description, const QPixmap& icon);
    void ShowChallengeIndicator(const QPixmap& icon, const QString& badgeName);
    void HideChallengeIndicator(const QString& badgeName = "");
    void ClearAll();

private:
    QLabel* m_leaderboardLabel = nullptr;
    QLabel* m_networkLabel = nullptr;
    ToastOverlay* m_overlay = nullptr;
};