#pragma once
#include <QWidget>
#include <QLabel>
#include <QPropertyAnimation>

class AchievementToast : public QWidget
{
    Q_OBJECT
public:
    AchievementToast(const QString& title,
                     const QString& desc,
                     const QPixmap& icon,
                     QWidget* parent = nullptr);

    void play();

private:
    QPropertyAnimation* m_anim;
};