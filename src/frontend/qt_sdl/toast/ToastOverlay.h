#pragma once

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QSoundEffect>
#include <QList>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>

class ToastWidget : public QWidget
{
    Q_OBJECT
public:
    ToastWidget(const QString& title,
                const QString& description,
                const QPixmap& icon,
                float scale,
                int maxWidth,
                QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
};

class ToastOverlay : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ToastOverlay(QWidget* parent = nullptr);
    void ShowToast(const QString& title, const QString& description, const QPixmap& icon, bool playSound = true);
    void ShowLeaderboardToast(const QString& title, const QString& description, const QPixmap& icon, bool playSound = true);
    void ShowChallenge(const QPixmap& icon, const QString& badgeName);
    void HideChallenge(const QString& badgeName = "");

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void RepositionToasts();
    void RepositionChallenge();
    QGraphicsScene* m_scene = nullptr;
    QList<QGraphicsProxyWidget*> m_toastProxies;
    QList<QGraphicsProxyWidget*> m_challengeProxies;
    QSoundEffect m_soundEffect;
};