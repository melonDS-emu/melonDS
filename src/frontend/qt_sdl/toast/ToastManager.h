#pragma once
#include <QObject>

class ToastOverlay;

class ToastManager : public QObject
{
    Q_OBJECT
public:
    static ToastManager& Get();

    void Init(QWidget* screenWidget);
    void ShowAchievement(const QString& title,
                         const QString& desc,
                         const QPixmap& icon);

private:
    ToastOverlay* m_overlay = nullptr;
};