#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QNetworkAccessManager>
#include <QHash>
#include <QPixmap>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QElapsedTimer>
#include <QPushButton>
#include <QVector>
#include <set>

class EmuInstance;
class AchievementItemWidget;

class RAOverlayWidget : public QGraphicsView
{
    Q_OBJECT
public:
    bool isOpen() const { return m_isOpen; }
    explicit RAOverlayWidget(EmuInstance* emu, QWidget* parent);
    ~RAOverlayWidget();

    void updateJoystick(uint32_t joyState);
    void refresh();
    void toggle();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    bool event(QEvent* e) override;

private:
    void ApplyRotationAndResize();
    std::set<QString> collapsedSections;
    uint32_t lastJoyState = 0;
    bool m_isOpen = false;
    void updateClock();
    void SetBadgeImage(AchievementItemWidget* item, const char* url);
    void LoadHeaderImage(QLabel* targetLabel, const char* url, QSize size);

    QWidget* CreateCollapsibleSection(const QString& title, int count, QVBoxLayout** innerLayout);

    EmuInstance* emuInstance;
    QNetworkAccessManager* netManager;

    QGraphicsScene* scene;
    QGraphicsProxyWidget* proxy;
    QWidget* contentWidget;

    QLabel* avatarLabel;
    QLabel* usernameLabel;
    QLabel* gameTitleLabel;
    QLabel* gameImgLabel;
    QLabel* timeLabel;
    QLabel* playtimeLabel;
    QWidget* listContainer;
    QVBoxLayout* listLayout;

    QHash<QString, QPixmap> badgeCache;
    QHash<QString, QPixmap> headerImageCache;
    QTimer* clockTimer;
    
    int m_currentRotation = 0;

    static constexpr int INITIAL_DELAY_MS = 400;
    static constexpr int REPEAT_DELAY_MS  = 80;
    QElapsedTimer repeatTimer;
    uint32_t heldMask = 0;
    bool repeating = false;
};