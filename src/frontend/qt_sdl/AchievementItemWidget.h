#pragma once
#include <QWidget>
#include <QString>

class QLabel;
class QProgressBar;
class QPaintEvent;

class AchievementItemWidget : public QWidget
{
    Q_OBJECT
public:
        void updateIcon(const QPixmap& pix);
        explicit AchievementItemWidget(
        const QString& title,
        const QString& description,
        bool unlocked,
        bool measured,
        const QString& progressText,
        float progressPercent,
        float uiScale,
        QWidget* parent = nullptr
    );

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int iconSize = 64;
    QLabel* titleLabel;
    QLabel* descLabel;
    QLabel* iconLabel;
    QProgressBar* progressBar;
};