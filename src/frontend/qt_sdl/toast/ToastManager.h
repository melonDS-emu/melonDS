#pragma once
#include <QObject>

class ToastOverlay;

class ToastManager : public QObject
{
    Q_OBJECT
public:
    explicit ToastManager(QObject* parent = nullptr);

    void Init(QWidget* screenWidget);
    void ShowAchievement(const QString& title,
                         const QString& desc,
                         const QPixmap& icon);
    void ShowChallengeIndicator(const QPixmap& icon);
    void HideChallengeIndicator();
    void ClearAll();

private:
    ToastOverlay* m_overlay = nullptr;
};