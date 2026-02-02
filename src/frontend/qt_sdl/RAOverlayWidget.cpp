#include "RAOverlayWidget.h"
#include "AchievementItemWidget.h"
#include <QKeyEvent>
#include "RetroAchievements/RAClient.h"
#include "EmuInstance.h"
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QPointer>
#include <QDebug>
#include <QDateTime> 
#include <QPainter>
#include <QPaintEvent>
#include "Config.h"
#include <cstdint>
#include <QPushButton>
#include <QGraphicsProxyWidget>
#include <QCoreApplication>
#include <set>

QWidget* RAOverlayWidget::CreateCollapsibleSection(const QString& title, int count, QVBoxLayout** innerLayout) {
    QPushButton* btn = new QPushButton();
    btn->setCheckable(true);
    
    btn->setStyleSheet(
        "QPushButton {"
        "   background-color: #333;"
        "   color: #eee;"
        "   border: 1px solid #444;"
        "   border-radius: 4px;"
        "   padding: 5px;"
        "   text-align: left;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #444;"
        "   border: 1px solid #666;"
        "}"
        "QPushButton:focus {"
        "   background-color: #555;"
        "   border: 2px solid #0078d7;"
        "}"
    );

    bool isCollapsed = collapsedSections.count(title) > 0;
    btn->setChecked(isCollapsed);

    QWidget* container = new QWidget();
    *innerLayout = new QVBoxLayout(container);
    (*innerLayout)->setContentsMargins(10, 0, 0, 5);
    (*innerLayout)->setSpacing(2);
    
    auto updateBtnText = [btn, title, count](bool collapsed) {
        btn->setText(QString("%1 %2 (%3)").arg(collapsed ? "▶" : "▼").arg(title).arg(count));
    };

    container->setVisible(!isCollapsed);
    updateBtnText(isCollapsed);

    listLayout->addWidget(btn);
    listLayout->addWidget(container);

    connect(btn, &QPushButton::toggled, [this, title, container, updateBtnText, count](bool checked) {
        container->setVisible(!checked);
        updateBtnText(checked);
        
        if (checked) collapsedSections.insert(title);
        else collapsedSections.erase(title);
    });

    return container;
}

RAOverlayWidget::RAOverlayWidget(EmuInstance* emu, QWidget* parent)
    : QGraphicsView(parent), emuInstance(emu)
{
    netManager = new QNetworkAccessManager(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::SubWindow); 
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background: rgba(0, 0, 0, 150); border: none;");
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::StrongFocus);

    scene = new QGraphicsScene(this);
    setScene(scene);

    scene->setBackgroundBrush(QColor(0, 0, 0, 150));

    contentWidget = new QWidget();
    contentWidget->setAttribute(Qt::WA_TranslucentBackground);
    contentWidget->setStyleSheet("background: rgba(0, 0, 0, 160); border: 1px solid rgba(255, 255, 255, 30); color: white;");

    if (parent && parent->window())
    parent->window()->installEventFilter(this);

    auto* root = new QVBoxLayout(contentWidget);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(15);

    auto* headerBar = new QHBoxLayout();
    headerBar->setSpacing(10);

    avatarLabel = new QLabel();
    avatarLabel->setFixedSize(48, 48);
    avatarLabel->setStyleSheet("background: #555; border-radius: 24px; border: 2px solid #ffcc00;");
    avatarLabel->setScaledContents(true);

    usernameLabel = new QLabel("Not Logged In");
    usernameLabel->setStyleSheet("color: white; font-size: 16px; font-weight: bold; background: transparent; border: none;");

    gameTitleLabel = new QLabel("No Game");
    gameTitleLabel->setStyleSheet("color: #ddd; font-size: 16px; background: transparent; border: none;");
    gameTitleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    gameImgLabel = new QLabel();
    gameImgLabel->setFixedSize(48, 48);
    gameImgLabel->setStyleSheet("background: #333; border-radius: 4px;");
    gameImgLabel->setScaledContents(true);

    timeLabel = new QLabel("--:--");
    timeLabel->setStyleSheet("color: #ffffff; font-size: 14px; font-family: monospace; background: transparent; border: none;");

    playtimeLabel = new QLabel("");
    playtimeLabel->setStyleSheet("color: #ffffff; font-size: 11px; background: transparent; border: none;");
    playtimeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    headerBar->addWidget(playtimeLabel);
    headerBar->addSpacing(10);
    headerBar->addWidget(avatarLabel);
    headerBar->addWidget(usernameLabel);
    headerBar->addStretch(); 
    headerBar->addWidget(gameTitleLabel);
    headerBar->addSpacing(10);
    headerBar->addWidget(gameImgLabel);
    headerBar->addSpacing(20);
    headerBar->addWidget(timeLabel);
    root->addLayout(headerBar);

    QLabel* title = new QLabel("Achievements");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 28px; font-weight: bold; color: #ffffff; margin-top: 5px; margin-bottom: 10px; background: transparent; border: none;");
    root->addWidget(title);
    
    QScrollArea* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    float scale = 1.0f;
    if (parent && parent->window()) {
        constexpr float refShortSide = 384.0f;
        float shortSide = std::min(parent->window()->width(), parent->window()->height());
        scale = std::clamp(shortSide / refShortSide, 0.65f, 1.25f);
    }

    int scrollWidth = std::clamp(int(10 * scale), 8, 14);
    int radius = scrollWidth / 2;

    scroll->setStyleSheet(QString(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical {"
        "    background: #2d2d2d;"
        "    width: %1px;"
        "    border-radius: %2px;"
        "    margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #555;"
        "    border-radius: %2px;"
        "    min-height: 25px;" 
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    ).arg(scrollWidth).arg(radius));

    listContainer = new QWidget;
    listLayout = new QVBoxLayout(listContainer);
    listLayout->setSpacing(5);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setAlignment(Qt::AlignTop);

    scroll->setWidget(listContainer);
    root->addWidget(scroll, 1); 

    proxy = scene->addWidget(contentWidget);

    if (auto* ra = emuInstance->getRA()) {
        ra->SetOnMeasuredProgress([this](unsigned, unsigned, unsigned, const char*) {
            QMetaObject::invokeMethod(this, [this]() { if (isVisible()) refresh(); });
        });
    }

    clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &RAOverlayWidget::updateClock);
    clockTimer->start(1000); 
    updateClock();
}

RAOverlayWidget::~RAOverlayWidget() {
    if (clockTimer) clockTimer->stop();
    if (emuInstance && emuInstance->getRA()) emuInstance->getRA()->SetOnMeasuredProgress(nullptr);
}

void RAOverlayWidget::toggle() {
    m_isOpen = !m_isOpen;
    if (isVisible()) {
        hide();
        emuInstance->overlayActive = false;
        if (parentWidget()) parentWidget()->setFocus(Qt::OtherFocusReason);
    } else {
        int instanceID = emuInstance->getInstanceID(); 
        Config::Table cfg = Config::GetLocalTable(instanceID);
        int rotMode = cfg.GetInt("Window0.ScreenRotation");
        m_currentRotation = rotMode * 90;

        if (parentWidget()) {
            QRect pRect = parentWidget()->rect();
            setGeometry(pRect);
            scene->setSceneRect(0, 0, pRect.width(), pRect.height());
            proxy->setRotation(0);
            proxy->setPos(0, 0);

            if (rotMode == 1 || rotMode == 3) {
                contentWidget->setFixedSize(pRect.height(), pRect.width());
                proxy->setRotation(m_currentRotation);
                if (rotMode == 1) proxy->setPos(pRect.width(), 0);
                else if (rotMode == 3) proxy->setPos(0, pRect.height());
            } else {
                contentWidget->setFixedSize(pRect.size());
                if (rotMode == 2) { proxy->setRotation(180); proxy->setPos(pRect.width(), pRect.height()); }
            }
        }
        ApplyRotationAndResize();
        refresh();
        show();
        raise();
        setFocus();
        emuInstance->overlayActive = true;
    }
}

void RAOverlayWidget::refresh() {
    RAContext* ra = emuInstance->getRA();
    if (!ra) return;

    QRect r = contentWidget->rect();
    int shortSide = std::min(r.width(), r.height());

    float uiScale = shortSide / 600.0f;
    uiScale = std::clamp(uiScale, 0.65f, 1.0f);
    
    if (ra->IsLoggedIn()) {
        usernameLabel->setText(QString::fromStdString(ra->GetUser()));
        const char* picUrl = ra->GetUserPicURL();
        if (picUrl) LoadHeaderImage(avatarLabel, picUrl, QSize(48, 48));
    }

    if (ra->IsGameLoaded()) {
        QString titleStr = QString::fromUtf8(ra->GetGameTitle());
        if (titleStr.length() > 30) titleStr = titleStr.left(27) + "...";
        gameTitleLabel->setText(titleStr);
        const char* iconUrl = ra->GetGameIconURL();
        if (iconUrl) LoadHeaderImage(gameImgLabel, iconUrl, QSize(48, 48));
    }

    int hours = 0;
    int mins = 0;
    
    if (ra->IsGameLoaded()) {
        auto* gameInfo = ra->GetCurrentGameInfo();
        uint32_t gameID = gameInfo ? gameInfo->id : 0;
        
        if (gameID != 0) {
            std::string gameKey = std::to_string(gameID);
            int totalSeconds = Config::GetRAPlaytimeTable().GetInt(gameKey);
            
            hours = totalSeconds / 3600;
            mins = (totalSeconds % 3600) / 60;
        }
    }

    QString playtimeStr;
    if (hours > 0) {
        playtimeStr = QString("Playtime: %1h %2m").arg(hours).arg(mins);
    } else {
        playtimeStr = QString("Playtime: %1m").arg(mins);
    }

    playtimeLabel->setText(playtimeStr);
    playtimeLabel->show();

    if (contentWidget) {
        contentWidget->setFocus();
    }

    QLayoutItem* child;
    while ((child = listLayout->takeAt(0)) != nullptr) {
        if (QWidget* w = child->widget()) {
            w->hide();
            w->setFocusPolicy(Qt::NoFocus);
            w->deleteLater();
        }
        delete child;
    }

    if (!ra->IsGameLoaded()) {
        QLabel* info = new QLabel("Load a game with RetroAchievements support.");
        info->setStyleSheet("color: #aaa; background: transparent; border: none;");
        info->setAlignment(Qt::AlignCenter);
        listLayout->addWidget(info);
    } else {
        const auto& achievements = ra->GetAllAchievements();
        if (!achievements.empty()) {
            QVector<const RAContext::FullAchievement*> unlockedList;
            QVector<const RAContext::FullAchievement*> inProgressList;
            QVector<const RAContext::FullAchievement*> lockedList;

            for (const auto& ach : achievements) {
                if (ach.unlocked) {
                    unlockedList.append(&ach);
                } else {
                    bool hasProgress = ach.measured || !ach.progressText.empty();
                    if (hasProgress) inProgressList.append(&ach);
                    else lockedList.append(&ach);
                }
            }

        if (!unlockedList.isEmpty()) {
            QVBoxLayout* uLayout = nullptr;
            CreateCollapsibleSection("Unlocked", unlockedList.size(), &uLayout);
            for (const auto* ach : unlockedList) {
                auto* item = new AchievementItemWidget(QString::fromUtf8(ach->title.c_str()), QString::fromUtf8(ach->description.c_str()), true, false, "", 0.0f, uiScale, listContainer);
                uLayout->addWidget(item);
                if (ach->badge_url && ach->badge_url[0] != '\0') SetBadgeImage(item, ach->badge_url);
            }
        }

        if (!inProgressList.isEmpty()) {
            QVBoxLayout* pLayout = nullptr;
            CreateCollapsibleSection("In Progress", inProgressList.size(), &pLayout);
            for (const auto* ach : inProgressList) {
                auto* item = new AchievementItemWidget(QString::fromUtf8(ach->title.c_str()), QString::fromUtf8(ach->description.c_str()), false, true, QString::fromStdString(ach->progressText), ach->measured_percent, uiScale, listContainer);
                pLayout->addWidget(item);
                if (ach->badge_locked_url && ach->badge_locked_url[0] != '\0') SetBadgeImage(item, ach->badge_locked_url);
            }
        }

        if (!lockedList.isEmpty()) {
            QVBoxLayout* lLayout = nullptr;
            CreateCollapsibleSection("Locked", lockedList.size(), &lLayout);
            for (const auto* ach : lockedList) {
                auto* item = new AchievementItemWidget(QString::fromUtf8(ach->title.c_str()), QString::fromUtf8(ach->description.c_str()), false, false, "", 0.0f, uiScale, listContainer);
                lLayout->addWidget(item);
                if (ach->badge_locked_url && ach->badge_locked_url[0] != '\0') SetBadgeImage(item, ach->badge_locked_url);
                }
            }
        }
    }
    listLayout->addStretch();
}

void RAOverlayWidget::updateJoystick(uint32_t joyState) {
    if (!isVisible()) return;

    constexpr int BIT_UP = 6, BIT_DOWN = 7, BIT_A = 0;
    
    uint32_t navMask = joyState & ((1 << BIT_UP) | (1 << BIT_DOWN));
    if ((navMask & ~heldMask) != 0) { repeatTimer.restart(); repeating = false; }
    heldMask = navMask;

    bool doRepeat = false;
    if (heldMask != 0) {
        if (!repeating && repeatTimer.elapsed() >= 200) { repeating = true; repeatTimer.restart(); doRepeat = true; }
        else if (repeating && repeatTimer.elapsed() >= 60) { repeatTimer.restart(); doRepeat = true; }
    }

    if (navMask != 0 || doRepeat) {
        Qt::Key key = (heldMask & (1 << BIT_UP)) ? Qt::Key_Backtab : Qt::Key_Tab;
        QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
        QCoreApplication::sendEvent(scene, &event);
    }

    bool isAPressed = (joyState & (1 << BIT_A));
    bool wasAPressed = (lastJoyState & (1 << BIT_A));

    if (isAPressed && !wasAPressed) { 
        if (auto* btn = qobject_cast<QPushButton*>(contentWidget->focusWidget())) {
            btn->click();
        }
    }

    if (auto* next = contentWidget->focusWidget()) {
        if (auto* scroll = contentWidget->findChild<QScrollArea*>()) {
            scroll->ensureWidgetVisible(next, 20, 20);
        }
    }

    if (navMask == 0) { heldMask = 0; repeating = false; }
    
    lastJoyState = joyState;
}

bool RAOverlayWidget::event(QEvent* e) {
    if (e->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(e);
        Config::Table cfg = Config::GetLocalTable(emuInstance->getInstanceID());
        
        int cfgUp = cfg.GetInt("Keyboard.Up");
        int cfgDown = cfg.GetInt("Keyboard.Down");
        int cfgA = cfg.GetInt("Keyboard.A");

        bool isUp = (ke->key() == cfgUp || ke->key() == Qt::Key_Up);
        bool isDown = (ke->key() == cfgDown || ke->key() == Qt::Key_Down);
        
        bool isConfirm = (ke->key() == cfgA || ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter);

        if (isUp || isDown) {
            Qt::Key key = isDown ? Qt::Key_Tab : Qt::Key_Backtab;
            QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
            QCoreApplication::sendEvent(scene, &event);
            return true;
        }

        if (isConfirm) {
            if (!ke->isAutoRepeat()) { 
                if (auto* btn = qobject_cast<QPushButton*>(contentWidget->focusWidget())) {
                    btn->click();
                    return true;
                }
            }
            return true; 
        }

        if (parentWidget()) {
            return QCoreApplication::sendEvent(parentWidget(), e);
        }
    }
    return QGraphicsView::event(e);
}

void RAOverlayWidget::updateClock() { 
    timeLabel->setText(QDateTime::currentDateTime().toString("HH:mm")); 

    if (isVisible()) {
        RAContext* ra = emuInstance->getRA();
        if (ra && ra->IsGameLoaded()) {
            
            auto* gameInfo = ra->GetCurrentGameInfo();
            uint32_t gameID = gameInfo ? gameInfo->id : 0;
            
            int totalSeconds = 0;
            if (gameID != 0) {
                std::string gameKey = std::to_string(gameID);
                totalSeconds = Config::GetRAPlaytimeTable().GetInt(gameKey);
            }

            int hours = totalSeconds / 3600;
            int mins = (totalSeconds % 3600) / 60;

            QString playtimeStr;
            if (hours > 0) {
                playtimeStr = QString("Playtime: %1h %2m").arg(hours).arg(mins);
            } else {
                playtimeStr = QString("Playtime: %1m").arg(mins);
            }

            playtimeLabel->setText(playtimeStr);
            if (playtimeLabel->isHidden()) playtimeLabel->show();
        } else {
            playtimeLabel->hide();
        }
    }
}

bool RAOverlayWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == parentWidget()->window() &&
    (event->type() == QEvent::Resize ||
        event->type() == QEvent::Move ||
        event->type() == QEvent::WindowStateChange))
    {
        if (isVisible()) {
            ApplyRotationAndResize();
        }
    }
    return QGraphicsView::eventFilter(obj, event);
}

void RAOverlayWidget::SetBadgeImage(AchievementItemWidget* item, const char* url) {
    if (!url || !*url) return;
    QString qurl = QString::fromUtf8(url);
    if (badgeCache.contains(qurl)) { item->updateIcon(badgeCache[qurl]); return; }
    QPointer<AchievementItemWidget> safeItem(item);
    QNetworkReply* reply = netManager->get(QNetworkRequest(QUrl(qurl)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, safeItem, qurl]() {
        reply->deleteLater();
        if (safeItem && reply->error() == QNetworkReply::NoError) {
            QPixmap pix; if (pix.loadFromData(reply->readAll())) { badgeCache.insert(qurl, pix); safeItem->updateIcon(pix); }
        }
    });
}

void RAOverlayWidget::LoadHeaderImage(QLabel* targetLabel, const char* url, QSize size) {
    if (!url || !*url || !targetLabel) return;
    QString qurl = QString::fromUtf8(url);
    if (headerImageCache.contains(qurl)) { targetLabel->setPixmap(headerImageCache[qurl]); targetLabel->setStyleSheet("background: transparent; border: none;"); return; }
    QPointer<QLabel> safeLabel(targetLabel);
    QNetworkReply* reply = netManager->get(QNetworkRequest(QUrl(qurl)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, safeLabel, qurl, size]() {
        reply->deleteLater();
        if (safeLabel && reply->error() == QNetworkReply::NoError) {
            QPixmap pix; if (pix.loadFromData(reply->readAll())) {
                QPixmap scaled = pix.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                headerImageCache.insert(qurl, scaled); safeLabel->setPixmap(scaled); safeLabel->setStyleSheet("background: transparent; border: none;");
            }
        }
    });
}
void RAOverlayWidget::ApplyRotationAndResize()
{
    if (!parentWidget()) return;

    int instanceID = emuInstance->getInstanceID();
    Config::Table cfg = Config::GetLocalTable(instanceID);
    int rotMode = cfg.GetInt("Window0.ScreenRotation");
    m_currentRotation = rotMode * 90;

    QRect pRect = parentWidget()->rect();
    setGeometry(pRect);
    scene->setSceneRect(0, 0, pRect.width(), pRect.height());

    proxy->setRotation(0);
    proxy->setPos(0, 0);

    if (rotMode == 1 || rotMode == 3) {
        contentWidget->setFixedSize(pRect.height(), pRect.width());
        proxy->setRotation(m_currentRotation);
        if (rotMode == 1) proxy->setPos(pRect.width(), 0);
        else proxy->setPos(0, pRect.height());
    } else {
        contentWidget->setFixedSize(pRect.size());
        if (rotMode == 2) {
            proxy->setRotation(180);
            proxy->setPos(pRect.width(), pRect.height());
        }
    }

    int shortSide = std::min(contentWidget->width(), contentWidget->height());
    bool mini = (shortSide < 420); 

    avatarLabel->setVisible(!mini);
    usernameLabel->setVisible(!mini);
    timeLabel->setVisible(!mini);
    gameTitleLabel->setVisible(!mini);

    if (auto* root = qobject_cast<QVBoxLayout*>(contentWidget->layout())) {
        if (mini) {
            root->setContentsMargins(10, 5, 10, 10);
            root->setSpacing(5);
        } else {
            root->setContentsMargins(20, 20, 20, 20);
            root->setSpacing(15);
        }
    }
}